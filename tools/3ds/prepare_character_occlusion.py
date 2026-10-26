#!/usr/bin/env python3
"""Bake local Pure3D character creases into color multipliers, without editing assets.

The output contains only one shade byte per source vertex and content signatures.
Ray tracing happens on the build computer. The game consumes these bytes at load.
"""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

from skin_inventory import chunks, pstring


def fingerprint(data):
    result = 2166136261
    for byte in data:
        result = ((result ^ byte) * 16777619) & 0xffffffff
    return result


def read_skins(path):
    data = path.read_bytes()

    def walk(start, end):
        for kind, body, child, stop in chunks(data, start, end):
            if kind == 0x10001:
                name, offset = pstring(data, body)
                groups = []
                for k, b, c, e in chunks(data, child, stop):
                    if k != 0x10002:
                        continue
                    material, o = pstring(data, b + 4)
                    topology, fmt, count, index_count, bones = struct.unpack_from('<5I', data, o)
                    sub = {sk: data[sb:sc] for sk, sb, sc, se in chunks(data, c, e)}
                    position, normal = sub.get(0x10005), sub.get(0x10006)
                    if not position or not normal or topology not in (0, 1):
                        groups.append(None)
                        continue
                    assert struct.unpack_from('<I', position)[0] == count
                    assert struct.unpack_from('<I', normal)[0] == count
                    vertices = [position[4+12*i:16+12*i] + normal[4+12*i:16+12*i]
                                for i in range(count)]
                    indices = sub.get(0x1000A)
                    indices = list(struct.unpack_from(f'<{index_count}I', indices, 4)) if indices else list(range(count))
                    if topology == 0:
                        assert len(indices) % 3 == 0
                        triangles = [indices[i:i+3] for i in range(0, len(indices), 3)]
                    else:
                        triangles = [[indices[i], indices[i+1], indices[i+2]] for i in range(len(indices)-2)]
                    assert all(max(triangle) < count for triangle in triangles)
                    signature = fingerprint(b''.join(vertices[i] for i in (0, count//2, count-1)))
                    groups.append(dict(vertices=vertices, triangles=triangles, signature=signature,
                                       material=material, authored_colour=0x10008 in sub))
                yield name, groups
            yield from walk(child, stop)

    return list(walk(0, len(data)))


def prepare(assets, helper, samples):
    records, sources, statistics = {}, {}, []
    for path in sorted(assets.glob('*.p3d')):
        skins = read_skins(path)
        if not skins:
            continue
        sources[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        for name, groups in skins:
            vertices, triangles, ranges = [], [], []
            for group in groups:
                ranges.append(len(vertices))
                if group:
                    triangles.extend([[i+len(vertices) for i in triangle] for triangle in group['triangles']])
                    vertices.extend(group['vertices'])
            if not vertices or not triangles:
                continue
            heights = [struct.unpack_from('<f', vertex, 4)[0] for vertex in vertices]
            radius = max(.08, min(.38, (max(heights)-min(heights))*.13))
            request = (struct.pack('<IIIf', len(vertices), len(triangles), samples, radius) +
                       b''.join(vertices) + b''.join(struct.pack('<3I', *t) for t in triangles))
            shades = subprocess.run([str(helper.resolve())], input=request, capture_output=True, check=True).stdout
            assert len(shades) == len(vertices)
            for number, group in enumerate(groups):
                if not group:
                    continue
                count = len(group['vertices'])
                key = (fingerprint(name.encode()), number, count, group['signature'])
                value = shades[ranges[number]:ranges[number]+count]
                if key in records and records[key] != value:
                    raise ValueError(f'Conflicting content key: {path}:{name}:{number}')
                records[key] = value
                statistics.append(dict(file=path.name, skin=name, group=number, vertices=count,
                                       minimum=min(value), mean=sum(value)/count, radius=radius))
    return records, sources, statistics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets', type=Path)
    parser.add_argument('--helper', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--samples', default=64, type=int)
    args = parser.parse_args()
    records, sources, statistics = prepare(args.assets, args.helper, args.samples)
    offset = 16 + 20*len(records)
    index, payload = bytearray(), bytearray()
    for key, shades in sorted(records.items()):
        index.extend(struct.pack('<5I', *key, offset+len(payload)))
        payload.extend(shades)
    blob = b'SRRAO001' + struct.pack('<II', len(records), offset+len(payload)) + index + payload
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(blob)
    report = dict(format='SRRAO001', samples=args.samples, records=len(records), shade_bytes=len(payload),
                  bytes=len(blob), sha256=hashlib.sha256(blob).hexdigest(), sources=sources, groups=statistics)
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('sources', 'groups')}))


if __name__ == '__main__':
    main()
