#!/usr/bin/env python3
"""Check local helper isolation and source-bound RGB restoration without game data."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root/'tools/3ds'))
spec = importlib.util.spec_from_file_location('v12_assets', root/'browser-builder/engine-conversion/v12-assets.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)
from prepare_character_detail import Node

original = subprocess.run
request = struct.pack('<IIIf', 3, 0, 64, 1.)+bytes(3*24)
seen = []
with assets._helper(lambda data, sun: seen.append((data, sun)) or bytes([1, 2, 3])):
    value = subprocess.run([str(assets.HELPER)], input=request, capture_output=True, check=True)
    assert value.stdout == bytes([1, 2, 3]) and seen == [(request, None)]
    for command in (['/bin/echo', 'unexpected'], [str(assets.HELPER), '--other'], 'not a command list'):
        try:
            subprocess.run(command, input=request)
        except RuntimeError:
            pass
        else:
            raise AssertionError(command)
assert subprocess.run is original
try:
    with assets._helper(lambda data, sun: bytes(2)):
        subprocess.run([str(assets.HELPER)], input=request, capture_output=True)
except ValueError:
    pass
else:
    raise AssertionError('Incomplete helper output was accepted.')
assert subprocess.run is original

data = Node(0x10008, struct.pack('<I', 1000)+bytes([30, 40, 50, 128])*1000, []).encode()
target = bytearray(data)
target[16] += 2
record = {'source_sha256': 'known-source', 'strict_sha256': hashlib.sha256(data).hexdigest(),
          'accepted_sha256': hashlib.sha256(target).hexdigest(), 'deltas': [[16, 2]]}
assert assets._round_scene(data, 'known-source', record) == (bytes(target), 1)
for source, change in [('different-source', [16, 2]), ('known-source', [19, 1]),
                       ('known-source', [8, 1]), ('known-source', [16, 5]),
                       ('known-source', [-1, 1]), ('known-source', [16, True])]:
    try:
        assets._round_scene(data, source, dict(record, deltas=[change]))
    except ValueError:
        pass
    else:
        raise AssertionError((source, change))
try:
    assets._round_scene(data, 'known-source', dict(record, deltas=[[16, 1]]*4))
except ValueError:
    pass
else:
    raise AssertionError('A nonsparse restoration was accepted.')
print('PASS: exact source/output fingerprints, sparse RGB-only signed deltas, rejected alpha/geometry, helper interception and restoration.')
