import sys, os

def spv_to_header(spv_path, var_name):
    with open(spv_path, 'rb') as f:
        data = f.read()
    hex_bytes = ', '.join(f'0x{b:02x}' for b in data)
    lines = [f'static const unsigned char {var_name}[] = {{']
    parts = hex_bytes.split(', ')
    for i in range(0, len(parts), 12):
        chunk = ', '.join(parts[i:i+12])
        lines.append(f'    {chunk},')
    lines[-1] = lines[-1].rstrip(',')
    lines.append('};')
    lines.append(f'static const size_t {var_name}_size = {len(data)};')
    return '\n'.join(lines) + '\n\n'

if __name__ == '__main__':
    output_path = sys.argv[1]
    vert_path = sys.argv[2]
    frag_path = sys.argv[3]
    content = '#pragma once\n// Auto-generated embedded shader SPIR-V data\n\n'
    content += spv_to_header(vert_path, 'sprite_vert_spv')
    content += spv_to_header(frag_path, 'sprite_frag_spv')
    with open(output_path, 'w') as f:
        f.write(content)
