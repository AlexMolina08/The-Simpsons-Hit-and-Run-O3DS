#!/usr/bin/env python3
"""Inventory every authored streaming gate; no gameplay or assets are changed."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
from skin_inventory import chunks, pstring


def gates(path):
    data = path.read_bytes()

    def walk(start, end):
        for kind, body, child, stop in chunks(data, start, end):
            if kind == 0x3000005:
                name, off = pstring(data, body)
                locator_type, words = struct.unpack_from('<2I', data, off)
                if locator_type == 5:
                    payload = data[off + 8:off + 8 + words * 4].split(b'\0', 1)[0].decode()
                    position = struct.unpack_from('<3f', data, off + 8 + words * 4)
                    actions = {'load': [], 'dump': [], 'interior_load': [], 'interior_dump': [],
                               'activate_sphere': [], 'deactivate_sphere': []}
                    names = {';': 'load', ':': 'dump', '@': 'interior_load', '$': 'interior_dump',
                             '*': 'activate_sphere', '&': 'deactivate_sphere'}
                    for filename, operation in re.findall(r'([^;:@$&*]+)([;:@$&*])', payload):
                        actions[names[operation]].append(filename)
                    volumes = []
                    for vk, vb, vc, ve in chunks(data, child, stop):
                        if vk != 0x3000006:
                            continue
                        volume_name, vo = pstring(data, vb)
                        shape, = struct.unpack_from('<I', data, vo)
                        scale = struct.unpack_from('<3f', data, vo + 4)
                        matrix = struct.unpack_from('<16f', data, vo + 16)
                        volumes.append(dict(name=volume_name, shape='sphere' if shape == 0 else 'box',
                                            extent_or_radius=scale, matrix=matrix, center=matrix[12:15]))
                    yield dict(name=name, position=position, actions=actions, volumes=volumes)
            yield from walk(child, stop)

    return list(walk(0, len(data)))


def audit(art, overlay):
    def resolve(name):
        p = overlay / name
        return p if p.is_file() else art / name

    terrains = {int(p.name[1]): resolve(p.name) for p in art.glob('*TERRA.p3d')}
    levels = []
    for level in sorted(terrains):
        source = terrains[level]
        entries = gates(source)
        for gate in entries:
            files = gate['actions']['load'] + gate['actions']['interior_load']
            gate['listed_bytes'] = sum(resolve(f).stat().st_size for f in files if resolve(f).is_file())
            gate['missing_files'] = [f for f in files if not resolve(f).is_file()]
            gate['external_files'] = [f for f in files if re.fullmatch(r'l[1-7][rz].*\.p3d', f)]
        levels.append(dict(level=level, source=str(source), source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
            gates=len(entries), volumes=sum(len(g['volumes']) for g in entries),
            distinct_exterior_sets=len({tuple(sorted(g['external_files'])) for g in entries if g['external_files']}),
            largest_exterior_file_list=max((len(g['external_files']) for g in entries), default=0),
            largest_listed_payload_bytes=max((g['listed_bytes'] for g in entries), default=0), entries=entries))
    return dict(levels=levels,
        interpretation='Authored requests, not runtime demand. Already resident files are skipped. Byte totals exclude prepared texture and other secondary I/O; they do not establish load latency or visibility.',
        next_evidence='Measure first trigger entry, enqueue, dispatch, first/last geometry insertion, section completion, culling and display dispatch during ordinary gameplay.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('art', type=Path)
    parser.add_argument('--overlay', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.art, args.overlay)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps([{k: v for k, v in row.items() if k not in ('entries', 'source', 'source_sha256')}
                      for row in result['levels']], indent=2))
