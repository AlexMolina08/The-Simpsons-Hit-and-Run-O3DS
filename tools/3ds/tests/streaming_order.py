#!/usr/bin/env python3
"""Verify road dependencies, verbatim chunks, collision order and stable probes."""
import argparse
from collections import Counter
from pathlib import Path
import struct
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/3ds'))
from prepare_streaming_order import reorder, top_chunks, STATIC, TEXTURE, SHADER, INTERSECT
import prepare_streaming_order as release_order
import hashlib
import os
from prepare_stream_probes import extract


def chunk(kind, body=b'', children=b''):
    return struct.pack('<3I', kind, 12 + len(body), 12 + len(body) + len(children)) + body + children


def text(value):
    value = value.encode()
    return bytes([len(value)]) + value


def shader(texture, translucent=0):
    return chunk(SHADER, text('road_material') + struct.pack('<I', 0) + text('worldsun') +
                 struct.pack('<4I', translucent, 0, 0, 1),
                 chunk(0x11002, struct.pack('<I', 0) + text(texture)))


ground = chunk(STATIC, text('trackShape_001') + struct.pack('<2I', 0, 0),
    chunk(0x10000, text('road') + struct.pack('<2I', 0, 1),
        chunk(0x10004, struct.pack('<4f', 1, 2, 3, 4)) +
        chunk(0x10002, struct.pack('<I', 0) + text('road_material') + bytes(20))))
terrain = chunk(0xff443350, children=chunk(TEXTURE, text('asphalt')))
blocks = [chunk(TEXTURE, text('billboard') + bytes(8000)), shader('asphalt'),
          chunk(INTERSECT, bytes(32)), chunk(0x121000, bytes(40)), ground]
original = chunk(0xff443350, children=b''.join(blocks))
ordered, audit = reorder(original, terrain)
_, top = top_chunks(ordered)
assert [entry[0] for entry in top[:3]] == [INTERSECT, SHADER, STATIC]
assert audit['road_meshes'] == 1 and audit['collision_chunks'] == 1
assert len(original) == len(ordered)
assert Counter(blocks) == Counter(ordered[body-12:end] for kind, body, child, end in top)
for invalid in (shader('missing'), shader('asphalt', 1)):
    try:
        reorder(chunk(0xff443350, children=invalid + ground), terrain)
    except ValueError:
        pass
    else:
        raise AssertionError('Unproven/translucent dependency accepted')
try:
    reorder(chunk(0xff443350, children=shader('asphalt') * 2 + ground), terrain)
except ValueError:
    pass
else:
    raise AssertionError('Ambiguous shader accepted')
alpha_ground = bytearray(ground)
struct.pack_into('<I', alpha_ground, 12 + len(text('trackShape_001')) + 4, 1)
for world, permanent in (
    (shader('asphalt') + bytes(alpha_ground), terrain),
    (shader('asphalt') + ground, chunk(0xff443350, children=chunk(TEXTURE, text('asphalt')) * 2)),
    (ground, chunk(0xff443350, children=shader('asphalt', 1) + chunk(TEXTURE, text('asphalt')))),
):
    try:
        reorder(chunk(0xff443350, children=world), permanent)
    except ValueError:
        pass
    else:
        raise AssertionError('Alpha entity or invalid permanent dependency accepted')
unchanged = chunk(0xff443350, children=blocks[0] + blocks[3])
assert reorder(unchanged, terrain)[0] == unchanged
with tempfile.TemporaryDirectory(prefix='stream-order-') as directory:
    path = Path(directory) / 'l2z2.p3d'
    path.write_bytes(original)
    before = extract(path)
    path.write_bytes(ordered)
    assert extract(path) == before

with tempfile.TemporaryDirectory(prefix='stream-order-package-') as directory:
    folder = Path(directory)
    original_art, staged_art = folder/'original', folder/'staged'
    original_art.mkdir(); staged_art.mkdir()
    fingerprints = release_order.RELEASE_FILES
    try:
        release_order.RELEASE_FILES = {name: (hashlib.sha256(original).hexdigest(),
            hashlib.sha256(ordered).hexdigest()) for name in ('l2z2.p3d', 'l2r2.p3d')}
        (staged_art/'l2_TERRA.p3d').write_bytes(terrain)
        for name in release_order.RELEASE_FILES:
            (original_art/name).write_bytes(original)
            os.link(original_art/name, staged_art/name)
        release_order.apply_release_order(staged_art)
        release_order.apply_release_order(staged_art)  # Packaging is idempotent.
        for name in release_order.RELEASE_FILES:
            assert (original_art/name).read_bytes() == original
            assert (staged_art/name).read_bytes() == ordered
        (staged_art/'l2z2.p3d').write_bytes(original)
        (staged_art/'l2r2.p3d').write_bytes(b'unsupported')
        try:
            release_order.apply_release_order(staged_art)
        except ValueError:
            assert (staged_art/'l2z2.p3d').read_bytes() == original
        else:
            raise AssertionError('Unsupported input changed a staged file')
    finally:
        release_order.RELEASE_FILES = fingerprints

parser = argparse.ArgumentParser()
parser.add_argument('--art', type=Path)
args = parser.parse_args()
if args.art:
    with tempfile.TemporaryDirectory(prefix='stream-order-real-') as directory:
        for name in ('l2z2.p3d', 'l2r2.p3d'):
            data = (args.art / name).read_bytes()
            result, audit = reorder(data, (args.art / 'l2_TERRA.p3d').read_bytes())
            path = Path(directory) / name
            path.write_bytes(result)
            assert sorted(extract(path), key=lambda p: p['key']) == sorted(extract(args.art / name), key=lambda p: p['key'])
            assert audit['road_meshes'] > 0 and audit['priority_prefix_bytes'] < audit['original_first_road_byte']
print('PASS: verbatim chunk multiset, collision-first order, validated road dependencies, stable geometry probes and rejection of unknown/duplicate/translucent dependencies.')
