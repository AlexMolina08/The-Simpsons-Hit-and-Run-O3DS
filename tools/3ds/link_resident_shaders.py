#!/usr/bin/env python3
"""Link existing vertex programs without changing their register allocation.

Picasso normally shares named uniform allocations between input files. These
programs deliberately overlap registers, so use each standalone header to pin
its aliases before linking. Instructions remain in their original order.
"""
import argparse
from pathlib import Path
import re
import struct
import subprocess


def pin(source, header, prefix):
    registers = {}
    for kind, name, number, bit in re.findall(
            r'#define VSH_(FVEC|IVEC|FLAG)_(\w+) (0x[0-9A-Fa-f]+|BIT\((\d+)\))', header):
        registers[name] = ('b' + bit if kind == 'FLAG' else
                           ('c' if kind == 'FVEC' else 'i') + str(int(number, 16)))
    if re.search(r'^\s*\.(const\w*|set\w*|gsh)\b', source, re.M):
        raise ValueError('Resident linking requires vertex programs without embedded constants')
    def declaration(match):
        name = match[2]
        return '.alias ' + name + ' ' + registers[name]
    source = re.sub(r'^\.(fvec|ivec|bool)\s+(\w+)(?:\[\d+\])?\s*$',
                    declaration, source, flags=re.M)
    if re.search(r'^\.(fvec|ivec|bool)\b', source, re.M):
        raise ValueError('Unsupported uniform declaration')
    # Procedures and labels share Picasso's global namespace; aliases do not.
    symbols = set(re.findall(r'^\.proc\s+(\w+)', source, re.M))
    symbols.update(re.findall(r'^(\w+):', source, re.M))
    return re.sub(r'\b\w+\b', lambda m: prefix + m[0] if m[0] in symbols else m[0], source)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--picasso', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('inputs', nargs=4, type=Path, metavar='SOURCE_OR_HEADER')
    args = parser.parse_args()
    sources = []
    for index in range(2):
        source, header = args.inputs[index * 2:index * 2 + 2]
        target = args.output.with_name('resident' + str(index) + '.v.pica')
        target.write_text(pin(source.read_text(), header.read_text(), 'resident' + str(index) + '_'))
        sources.append(str(target))
    subprocess.run([args.picasso, '-o', str(args.output), *sources], check=True)
    binary = args.output.read_bytes()
    dvlp = binary.index(b'DVLP')
    instructions = struct.unpack_from('<I', binary, dvlp + 12)[0]
    descriptors = struct.unpack_from('<I', binary, dvlp + 20)[0]
    # Citro3D reloads code on program switches at >=512 instructions, even if
    # DVLP is shared. Require the smaller resident range explicitly.
    if instructions >= 512 or descriptors > 128:
        raise ValueError(f'Resident shader limit exceeded: {instructions} instructions, {descriptors} descriptors')
    print(f'Resident programs: {instructions} instructions, {descriptors} descriptors')


if __name__ == '__main__':
    main()
