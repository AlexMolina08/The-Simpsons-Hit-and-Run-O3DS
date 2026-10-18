#!/usr/bin/env python3
"""Compare assembled instructions, operands and flow targets before/after linking."""
from pathlib import Path
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
picasso = '/opt/devkitpro/tools/bin/picasso'


def read(path):
    data = path.read_bytes()
    word = lambda offset: struct.unpack_from('<I', data, offset)[0]
    base = data.index(b'DVLP')
    code = struct.unpack_from('<' + 'I' * word(base + 12), data, base + word(base + 8))
    descriptors = [word(base + word(base + 16) + i * 8) for i in range(word(base + 20))]
    entries = []
    for i in range(word(4)):
        offset = word(8 + i * 4)
        output = offset + word(offset + 40)
        entries.append((word(offset + 8), word(offset + 12),
                        data[output:output + word(offset + 44) * 8]))
        assert word(offset + 28) == 0, 'Embedded constants require separate validation'
    return code, descriptors, entries


def canonical(words, descriptors, base):
    result = []
    for word in words:
        opcode = word >> 26
        if opcode < 0x20 or opcode >= 0x2e:
            mask = 31 if opcode >= 0x30 else 127
            descriptor = descriptors[word & mask]
            # Picasso merges descriptors whose unused lanes differ. Compare
            # consumed lanes, not don't-care bits in the shared table.
            compare = opcode in (0x2e, 0x2f)
            destination = 0 if compare else descriptor & 15
            lanes = [i for i in range(4) if destination & (8 >> i)]
            if opcode == 1: lanes = [0, 1, 2]  # dp3
            if opcode == 2: lanes = [0, 1, 2, 3]  # dp4
            if opcode in (0x0e, 0x0f): lanes = [0]  # rcp, rsq
            if compare: lanes = [0, 1]
            sources = 3 if opcode >= 0x30 else 1 if opcode in (0x0e, 0x0f, 0x12, 0x13) else 2
            operands = tuple(((descriptor >> (4 + source * 9)) & 1,
                              tuple((descriptor >> (11 + source * 9 - lane * 2)) & 3 for lane in lanes))
                             for source in range(sources))
            result.append((word & ~mask, destination, operands))
        elif opcode in (*range(0x24, 0x2a), 0x2c, 0x2d):
            result.append((word & ~(0xfff << 10), ((word >> 10) & 0xfff) - base))
        else:
            assert opcode in (0x20, 0x21, 0x22, 0x23), hex(opcode)
            result.append((word,))
    return result


with tempfile.TemporaryDirectory(prefix='srr-resident-') as directory:
    target = Path(directory)
    shaders = root / 'libs/pure3d/pddi/pica/shaders'
    args = []
    references = []
    for index, name in enumerate(('pddi_directional.v.pica', 'skin_eye_directional.v.pica')):
        source, header, binary = shaders / name, target / f'{index}.h', target / f'{index}.shbin'
        subprocess.run([picasso, '-o', str(binary), '-h', str(header), str(source)], check=True)
        args.extend((str(source), str(header)))
        references.append(read(binary))
    binary = target / 'resident.shbin'
    subprocess.run(['python3', str(root / 'tools/3ds/link_resident_shaders.py'),
                    '--picasso', picasso, '--output', str(binary), *args], check=True)
    code, descriptors, entries = read(binary)
    assert len(entries) == 2 and len(code) < 512 and len(descriptors) <= 128
    offset = 0
    for index, (original, operands, entry) in enumerate(references):
        assert entries[index] == (entry[0][0] + offset, entry[0][1] + offset, entry[0][2])
        assert canonical(original, operands, 0) == canonical(code[offset:offset + len(original)], descriptors, offset)
        offset += len(original)
    assert offset == len(code)
    print(f'PASS: all {len(code)} assembled instructions, operands, registers, flow targets and output maps match')
