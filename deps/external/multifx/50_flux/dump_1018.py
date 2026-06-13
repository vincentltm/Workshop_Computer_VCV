import struct
import sys

def dump_uf2(filename):
    with open(filename, 'rb') as f:
        data = f.read()

    blocks = len(data) // 512
    found = False
    for i in range(blocks):
        offset = i * 512
        block = data[offset:offset+512]
        
        magic0, magic1, flags, target_addr, payload_size, block_no, num_blocks, family_id = struct.unpack('<IIIIIIII', block[:32])
        
        if target_addr == 0x10180000:
            print(f"Found block at 0x10180000! Block No: {block_no}")
            print("Payload first 32 bytes:", block[32:64].hex())
            found = True
            break
            
if __name__ == '__main__':
    dump_uf2(sys.argv[1])
