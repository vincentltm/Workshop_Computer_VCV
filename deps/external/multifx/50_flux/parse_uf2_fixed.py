import struct
import sys

def parse_uf2(filename):
    with open(filename, 'rb') as f:
        data = f.read()

    blocks = len(data) // 512
    print(f"Total blocks in file: {blocks}")

    addr_map = []
    
    last_addr = -1
    for i in range(blocks):
        offset = i * 512
        block = data[offset:offset+512]
        
        magic0, magic1, flags, target_addr, payload_size, block_no, num_blocks, family_id = struct.unpack('<IIIIIIII', block[:32])
        
        if magic0 != 0x0A324655 or magic1 != 0x9E5D5157:
            print(f"Invalid magic at block {i}")
            continue
            
        if last_addr == -1 or target_addr != last_addr + 256:
            addr_map.append((i, target_addr, block_no, num_blocks))
            print(f"Jump to 0x{target_addr:08X} at block {i}")
            
        last_addr = target_addr
            
    # print final block
    print(f"Final block {blocks-1}: 0x{last_addr:08X}")

if __name__ == '__main__':
    parse_uf2(sys.argv[1])
