#!/usr/bin/env python3
"""Check retail HOME Menu audio extraction and reject malformed archives."""
from pathlib import Path
import struct
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from prepare_home_menu import archive_member, TRACK, DURATION, RATE


def fixture(payload=b'RSD4PCM fixture'):
    name = TRACK.upper()
    key = 0
    for char in name:
        value = ord(char)
        value = value + 32 if value < ord('a') else value
        key = (31 * key + value) & 0xffffffff
    data = bytearray(128 + len(payload))
    data[:22] = b'RADCORE CEMENT LIBRARY\0'
    data[32:36] = bytes((1, 2, 0, 1))
    struct.pack_into('<I', data, 44, 64)
    struct.pack_into('<4I', data, 64, 1, 0, 128, 0)
    struct.pack_into('<3I', data, 80, key, 128, len(payload))
    data[128:] = payload
    return data


with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'music02.rcf'
    valid = fixture()
    path.write_bytes(valid)
    assert archive_member(path, TRACK) == b'RSD4PCM fixture'
    for kind in ('magic', 'endian', 'header', 'count', 'member', 'truncated', 'missing'):
        data = bytearray(valid)
        if kind == 'magic': data[0] = 0
        elif kind == 'endian': data[34] = 1
        elif kind == 'header': struct.pack_into('<I', data, 44, len(data))
        elif kind == 'count': struct.pack_into('<I', data, 64, 100001)
        elif kind == 'member': struct.pack_into('<I', data, 88, len(data))
        elif kind == 'truncated': data = data[:40]
        elif kind == 'missing': struct.pack_into('<I', data, 80, 0)
        path.write_bytes(data)
        try:
            archive_member(path, TRACK)
        except ValueError:
            pass
        else:
            raise AssertionError('Accepted malformed archive: ' + kind)
assert DURATION < 3 and RATE == 32000
print('PASS: retail cue lookup is case-insensitive; invalid archive headers, directory bounds and member bounds are rejected.')
