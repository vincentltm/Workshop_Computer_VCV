import struct
import sys

def parse_uf2(filename):
    with open(filename, 'rb') as f:
        data = f.read()

    blocks = len(data) // 512
    print(f"Total blocks in file: {blocks}")

    addr_map = []
    
    for i in range(blocks):
        offset = i * 512
        block = data[offset:offset+512]
        
        magic0, magic1, flags, target_addr, payload_size, block_no, num_blocks, family_id = struct.unpack('<IIIIIIII', block[:32])
        
        if magic0 != 0x0A324655 or magic1 != 0x9E5D5157:
            print(f"Invalid magic at block {i}")
            continue
            
        if len(addr_map) == 0 or i == blocks - 1 or (target_addr != addr_map[-1][1] + 256 and target_addr != addr_map[-1][1]):
            addr_map.append((i, target_addr, block_no, num_blocks))
            
    print("Address jumps:")
    for jump in addr_map:
        print(f"FileBlock {jump[0]}: target_addr=0x{jump[1]:08X}, block_no={jump[2]}, num_blocks={jump[3]}")

if __name__ == '__main__':
    parse_uf2(sys.argv[1])
