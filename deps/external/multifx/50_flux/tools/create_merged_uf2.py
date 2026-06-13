# import audioop # Removed
import struct
import wave
import subprocess
import glob
import os

# Configuration
FIRMWARE_INPUT = os.getenv("FIRMWARE_INPUT", "../build/flux.uf2")
AUDIO_DIR = os.getenv("AUDIO_DIR", "../default_samples")
OUTPUT_FILE = os.getenv("OUTPUT_FILE", "../build/integrated.uf2")
TARGET_FLASH_OFFSET = 0x180000 # 1.5MB Offset
TARGET_ADDRESS = 0x10000000 + TARGET_FLASH_OFFSET
SETTINGS_ADDRESS = TARGET_ADDRESS - 0x1000  # one 4KB sector before samples
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
RP2040_FAMILY_ID = 0xE48BFF56
MFXS_MAGIC = 0x53584660 # Matches FLASH_MAGIC in SynthCore.h

# Manual Mu-Law Encoder (G.711)
def lin2ulaw_sample(val):
    sign = 0x80 if val < 0 else 0
    if val < 0: val = -val
    val = val >> 2 # 16-bit signed -> 14-bit magnitude
    if val > 8159: val = 8159
    val += 33
    
    exponent = 7
    for i in range(7, -1, -1):
        if val & (1 << (i + 5)):
            exponent = i
            break
            
    mantissa = (val >> (exponent + 1)) & 0x0F
    return (~(sign | (exponent << 4) | mantissa)) & 0xFF

def encode_mulaw(samples):
    out = bytearray()
    for s in samples:
        s = int(s)
        if s > 32767: s = 32767
        elif s < -32768: s = -32768
        out.append(lin2ulaw_sample(s))
    return out

def convert_audio(infile):
    outfile = "temp_conv.wav"
    # Convert to 24000Hz Mono 16-bit
    print(f"Converting {infile}...")
    # ffmpeg -i input -ar 24000 -ac 1 -c:a pcm_s16le output -y
    cmd = ["ffmpeg", "-i", infile, "-ar", "24000", "-ac", "1", "-c:a", "pcm_s16le", "-y", outfile]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) # Quiet output
    
    w = wave.open(outfile, 'rb')
    frames = w.readframes(w.getnframes())
    # samples = struct.unpack(f"<{len(frames)//2}h", frames) 
    # unpack not strictly needed if we feed bytes to audioop, but good for counting
    num_samples = len(frames) // 2
    samples = struct.unpack(f"<{num_samples}h", frames)
    w.close()
    
    # Encode
    mulaw = encode_mulaw(samples)
    
    print(f"  Samples: {num_samples}, Mulaw Size: {len(mulaw)}")
    os.remove(outfile)
    
    # Return count and data
    return num_samples, mulaw

def create_uf2_block(addr, data, blockno, numblocks):
    data = data.ljust(476, b'\0') # Payload is 476 bytes
    # Wait, UF2 payload is 256 bytes for Flash!
    # Family ID present? Yes.
    # Flags = 0x00002000 (FamilyID present)
    # Payload = 256 bytes.
    
    buf = bytearray(512)
    struct.pack_into("<IIII", buf, 0, UF2_MAGIC_START0, UF2_MAGIC_START1, 0x00002000, addr)
    struct.pack_into("<IIII", buf, 16, 256, blockno, numblocks, RP2040_FAMILY_ID)
    
    # Payload
    for i in range(256):
        if i < len(data):
            buf[32+i] = data[i]
    
    struct.pack_into("<I", buf, 508, UF2_MAGIC_END)
    return buf

def main():
    # 1. Read Firmware Blocks & Find End Address
    firmware_blocks = []
    max_addr = 0
    with open(FIRMWARE_INPUT, "rb") as f:
        data = f.read()
        for i in range(0, len(data), 512):
            block = data[i:i+512]
            firmware_blocks.append(block)
            # Address is at offset 12
            addr = struct.unpack_from("<I", block, 12)[0]
            if addr > max_addr: max_addr = addr
            
    print(f"Loaded Firmware: {len(firmware_blocks)} blocks")
    # Firmware usually ends at max_addr + 256
    firmware_end = max_addr + 256
    print(f"Firmware ends at: 0x{firmware_end:08x}")
    
    # 2. Process Audio
    sample_files = sorted(glob.glob(os.path.join(AUDIO_DIR, "*.wav")))
    processed_samples = []
    total_content_size = 0
    
    for f in sample_files:
        n_samples, adpcm = convert_audio(f)
        processed_samples.append((n_samples, adpcm))
        # Size = 4 bytes length + 8 bytes header + adpcm data + padding
        total_content_size += 4 + 8 + len(adpcm)
        while total_content_size % 4 != 0: total_content_size += 1
        
    print(f"Total Content Size: {total_content_size} bytes")
    
    # 3. Build Content Buffer
    content = bytearray(total_content_size)
    ptr = 0
    for n_samples, adpcm in processed_samples:
        # 1. Length (4 bytes) -> Total Block Size (Header + Data)
        struct.pack_into("<I", content, ptr, n_samples + 8)
        ptr += 4
        
        # 2. Loop Header (8 bytes)
        # LoopStart = 0
        struct.pack_into("<I", content, ptr, 0)
        ptr += 4
        # LoopEnd = n_samples (Play whole file)
        struct.pack_into("<I", content, ptr, n_samples)
        ptr += 4
        
        # 3. Data
        content[ptr:ptr+len(adpcm)] = adpcm
        ptr += len(adpcm)
        
        # Padding
        while ptr % 4 != 0: ptr += 1
        
    # 4. Calculate Gap Blocks (Bridge)
    target_addr = TARGET_ADDRESS
    if target_addr <= firmware_end:
        raise SystemExit(
            f"Firmware overlaps sampler flash region: "
            f"firmware_end=0x{firmware_end:08x} target_addr=0x{target_addr:08x}. "
            f"Refusing to shift sampler offsets."
        )

    if SETTINGS_ADDRESS <= firmware_end:
        raise SystemExit(
            f"Firmware overlaps settings flash sector: "
            f"firmware_end=0x{firmware_end:08x} SETTINGS_ADDRESS=0x{SETTINGS_ADDRESS:08x}. "
            f"Refusing to shift sampler offsets."
        )
        
    gap_size = target_addr - firmware_end
    gap_blocks_count = gap_size // 256
    skip_settings_gap_blocks = 0
    for i in range(gap_blocks_count):
        addr = firmware_end + (i * 256)
        if SETTINGS_ADDRESS <= addr < SETTINGS_ADDRESS + 4096:
            skip_settings_gap_blocks += 1
    print(f"Gap Size: {gap_size} bytes ({gap_blocks_count} blocks)")
    
    # 5. Calculate Total Blocks
    sample_data_blocks = (total_content_size + 255) // 256
    wipe_blocks = 16
    total_blocks = len(firmware_blocks) + gap_blocks_count + 2 + sample_data_blocks + wipe_blocks - skip_settings_gap_blocks  # +1 metadata +1 settings

    # 2MB card: flash is [0x10000000, 0x10200000)
    FLASH_START = 0x10000000
    FLASH_SIZE_BYTES = 2 * 1024 * 1024
    FLASH_END = FLASH_START + FLASH_SIZE_BYTES

    # Highest address written by sample blocks + wipe blocks
    # last_addr = TARGET_ADDRESS + 256 + (sample_data_blocks + wipe_blocks - 1)*256
    # last_byte = last_addr + 255
    last_written_end_exclusive = TARGET_ADDRESS + 256 + (sample_data_blocks + wipe_blocks) * 256
    if last_written_end_exclusive > FLASH_END:
        raise SystemExit(
            f"Audio/sampler payload too large for 2MB flash: "
            f"end_exclusive=0x{last_written_end_exclusive:08x} > 0x{FLASH_END:08x}"
        )
    
    print(f"New Total Blocks: {total_blocks}")
    
    with open(OUTPUT_FILE, "wb") as out:
        block_idx = 0
        
        # Firmware
        for block in firmware_blocks:
            data = bytearray(block)
            struct.pack_into("<II", data, 20, block_idx, total_blocks)
            out.write(data)
            block_idx += 1
            
        # Gap (Padding)
        dummy_payload = b'\xFF' * 476 # Fill with FF
        for i in range(gap_blocks_count):
            addr = firmware_end + (i * 256)
            if SETTINGS_ADDRESS <= addr < SETTINGS_ADDRESS + 4096:
                continue  # skip entire settings 4KB sector
            # Use 256 byte payload length
            block = create_uf2_block(addr, dummy_payload, block_idx, total_blocks)
            out.write(block)
            block_idx += 1
            
        # Metadata Block
        meta_payload = bytearray(256)
        struct.pack_into("<II", meta_payload, 0, MFXS_MAGIC, len(processed_samples))
        
        # Default Settings (Offset 8)
        # Struct: VP, P1M, P1C, P1A, P2M, P2C, P2A, C1M, C1C, C1A, C2M, C2C, C2A, KM, KX, KY
        defaults = [
            1,          # VP
            3, 0, 60,   # P1: AudioGate(3), Ch0, Arg60(Note)
            0, 0, 0,    # P2: MsgGate(0), Ch0, Arg0
            3, 0, 0,    # C1: AudioPitch(3), Ch0, Arg0
            6, 0, 1,    # C2: AudioLFO(6), Ch0, Arg1
            1, 2, 3     # Knobs
        ]
        for i, val in enumerate(defaults):
            meta_payload[8+i] = val
        
        meta_block = create_uf2_block(target_addr, meta_payload, block_idx, total_blocks)
        out.write(meta_block)
        block_idx += 1

        # Settings block in its own flash sector (copy same 256-byte payload layout)
        settings_block = create_uf2_block(SETTINGS_ADDRESS, meta_payload, block_idx, total_blocks)
        out.write(settings_block)
        block_idx += 1
        
        # Sample Data Blocks
        for i in range(sample_data_blocks):
            chunk = content[i*256 : (i+1)*256]
            addr = TARGET_ADDRESS + 256 + (i * 256)
            block = create_uf2_block(addr, chunk, block_idx, total_blocks)
            out.write(block)
            block_idx += 1
            
        # Wipe Blocks (Erases next 4KB flash sector to sever old samples)
        dummy_wipe = b'\x00' * 476
        for i in range(wipe_blocks):
            addr = TARGET_ADDRESS + 256 + ((sample_data_blocks + i) * 256)
            block = create_uf2_block(addr, dummy_wipe, block_idx, total_blocks)
            out.write(block)
            block_idx += 1
            
    print(f"Created {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
