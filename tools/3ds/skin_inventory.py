#!/usr/bin/env python3
"""Read uncompressed Pure3D assets without modifying or redistributing them."""
import argparse
import json
import struct
from pathlib import Path


def chunks(data, start=0, end=None):
    end = len(data) if end is None else end
    while start < end:
        kind, header, size = struct.unpack_from('<III', data, start)
        if not 12 <= header <= size or start + size > end:
            raise ValueError(f'invalid chunk at {start}')
        yield kind, start + 12, start + header, start + size
        start += size


def pstring(data, offset):
    size = data[offset]
    return data[offset+1:offset+1+size].rstrip(b'\0').decode(), offset+1+size


def skins(path):
    data = path.read_bytes()
    def walk(start, end):
        for kind, body, child, stop in chunks(data, start, end):
            if kind == 0x10001:
                name, offset = pstring(data, body)
                skeleton, offset = pstring(data, offset + 4)
                groups = []
                for k, b, c, e in chunks(data, child, stop):
                    if k != 0x10002:
                        continue
                    material, o = pstring(data, b + 4)
                    topology, fmt, vertices, indices, bones = struct.unpack_from('<5I', data, o)
                    sub = {sk: data[sb:sc] for sk, sb, sc, se in chunks(data, c, e)}
                    matrix_data = sub.get(0x1000B, b'')
                    weights = sub.get(0x1000C)
                    used, rigid = set(), 0
                    for i in range(vertices):
                        ids = matrix_data[4+4*i:8+4*i][::-1]
                        w = struct.unpack_from('<3f', weights, 4+12*i) if weights else (1, 0, 0)
                        rigid += w[0] == 1
                        used.update(ids[:1] if w[0] == 1 else ids)
                    groups.append(dict(material=material, topology=topology, format=fmt,
                        vertices=vertices, indices=indices, bones=bones, referenced_bones=len(used),
                        rigid_vertices=rigid, palette_eligible=0 < bones <= 23,
                        additional_linear_bytes=32+56*vertices if 0 < bones <= 23 else 0))
                yield dict(file=str(path), skin=name, skeleton=skeleton, groups=groups)
            yield from walk(child, stop)
    return list(walk(0, len(data)))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('assets', type=Path, nargs='+')
    args = parser.parse_args()
    print(json.dumps([skin for path in args.assets for skin in skins(path)], indent=2))
