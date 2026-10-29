#!/usr/bin/env python3
"""Extract independent scenery probes from existing art without changing assets."""
import argparse
import hashlib
import json
import math
import re
import struct
from pathlib import Path
from skin_inventory import chunks, pstring


def key(text, suffix=b''):
    value = 2166136261
    for byte in text.encode() + suffix:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def uid(text):
    value = 0
    for byte in text.encode():
        value = ((value * 65599) ^ byte) & 0xffffffffffffffff
    return value


def binary_key(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def extract(path):
    data = path.read_bytes()

    def walk(start, end):
        for kind, body, child, stop in chunks(data, start, end):
            if kind == 0x3f00000:
                name, _ = pstring(data, body)
                for k, b, c, e in chunks(data, child, stop):
                    if k != 0x10000:
                        continue
                    for sk, sb, sc, se in chunks(data, c, e):
                        if sk == 0x10004:
                            if sc - sb != 16:
                                raise ValueError(f"Invalid sphere: {path}/{name}")
                            sphere = struct.unpack_from('<4f', data, sb)
                            if not all(map(math.isfinite, sphere)) or sphere[3] <= 0:
                                raise ValueError(f"Invalid bounds: {path}/{name}")
                            yield dict(key=binary_key(struct.pack('<QQ4f', uid(path.name), uid(name), *sphere)), zone=binary_key(struct.pack('<Q', uid(path.name))),
                                       level=int(path.name[1]), sphere=sphere,
                                       file=path.name, entity=name)
            yield from walk(child, stop)

    return list(walk(0, len(data)))


def prepare(art, output):
    files = sorted(p for p in art.glob('*.p3d')
                   if re.fullmatch(r'l[1-7][rz].*\.p3d', p.name))
    probes = sorted((r for p in files for r in extract(p)), key=lambda r: r['key'])
    if not 0 < len(probes) <= 8192:
        raise ValueError(f"Probe count outside runtime capacity: {len(probes)}")
    unique = {}
    duplicate_copies = 0
    for record in probes:
        prior = unique.get(record['key'])
        if prior:
            if prior != record:
                raise ValueError("Conflicting probe key; do not conflate scenery")
            duplicate_copies += 1
        unique[record['key']] = record
    probes = list(unique.values())
    if {r['level'] for r in probes} != set(range(1, 8)):
        raise ValueError("The source catalog must cover all seven maps")
    output.mkdir(parents=True, exist_ok=True)
    binary = struct.pack('<4I', 0x32504753, 2, len(probes), 28)
    binary += b''.join(struct.pack('<3I4f', r['key'], r['zone'], r['level'], *r['sphere']) for r in probes)
    (output / 'stream-probes.bin').write_bytes(binary)
    manifest = dict(version=2, sha256=hashlib.sha256(binary).hexdigest(),
                    probe_count=len(probes), identical_duplicate_copies=duplicate_copies, probes=probes,
                    sources={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
                    coverage='StaticEntityDSG mesh bounds only; excludes instances, animated and dynamic objects')
    (output / 'stream-probes.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return {level: sum(r['level'] == level for r in probes) for level in range(1, 8)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('art', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.art, args.output), indent=2))
