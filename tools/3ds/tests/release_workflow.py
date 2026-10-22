#!/usr/bin/env python3
"""Check CIA version limits and the integrity gates used by resumed uploads."""
import hashlib
import json
from pathlib import Path
import sys
import struct
import tempfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from release_version import parse, next_version, packed_version, read_cia_version
from release_cia import verify_artifact, wait_for_release

assert parse('v1.2.3-rc.4') == ('1.2.3-rc.4', (1, 2, 3))
assert parse('63.63.15')[1] == (63, 63, 15)
for invalid in ('0.1.16', '64.0.0', '0.64.0', '1.2', '1.2.03', '1.2.3;echo bad'):
    try:
        parse(invalid)
    except ValueError:
        pass
    else:
        raise AssertionError('Accepted an invalid CIA version: ' + invalid)
assert next_version(['v0.1.1-rc.1']) == '0.1.2-rc.1'
assert next_version(['v1.2.15']) == '1.3.0-rc.1'
assert next_version(['v1.63.15']) == '2.0.0-rc.1'
assert packed_version('1.1.0') == 1040
assert packed_version('63.63.15') == 65535

def cia_fixture(version):
    # Deliberately unaligned section lengths exercise actual CIA section layout.
    offset = 0x2040 + 0x40 + 0x40
    data = bytearray(offset + 0x204)
    struct.pack_into('<IHHIII', data, 0, 0x2020, 0, 0, 1, 3, 0x204)
    struct.pack_into('>I', data, offset, 0x10004)
    struct.pack_into('>H', data, offset + 0x140 + 0x9c, version)
    return data
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    cia, manifest = root / 'game.cia', root / 'game.manifest.json'
    cia.write_bytes(cia_fixture(1059))
    assert read_cia_version(cia) == 1059
    metadata = {'source_commit': 'source-one', 'version': '1.2.3', 'cia_bytes': cia.stat().st_size,
                'cia_sha256': hashlib.sha256(cia.read_bytes()).hexdigest()}
    manifest.write_text(json.dumps(metadata))
    verify_artifact(cia, manifest, 'source-one')
    try:
        verify_artifact(cia, manifest, 'source-two')
    except RuntimeError:
        pass
    else:
        raise AssertionError('Resume accepted a different source revision')
    cia.write_bytes(b'changed release bytes')
    try:
        verify_artifact(cia, manifest, 'source-one')
    except RuntimeError:
        pass
    else:
        raise AssertionError('Resume accepted changed CIA bytes')
    cia.write_bytes(cia_fixture(1024))
    metadata['cia_sha256'] = hashlib.sha256(cia.read_bytes()).hexdigest()
    manifest.write_text(json.dumps(metadata))
    try:
        verify_artifact(cia, manifest, 'source-one')
    except RuntimeError:
        pass
    else:
        raise AssertionError('Resume accepted the wrong embedded title version with a matching checksum')
    for truncated in (b'', b'\0' * 32, cia_fixture(1059)[:-1]):
        cia.write_bytes(truncated)
        try:
            read_cia_version(cia)
        except ValueError:
            pass
        else:
            raise AssertionError('Accepted missing CIA title metadata')
with patch('release_cia.release_info', side_effect=[None, None, {'assets': []}]), patch('release_cia.time.sleep'):
    assert wait_for_release('owner/repo', 'v1.2.3') == {'assets': []}
with patch('release_cia.release_info', return_value=None), patch('release_cia.time.sleep'):
    try:
        wait_for_release('owner/repo', 'v1.2.3')
    except RuntimeError:
        pass
    else:
        raise AssertionError('An invisible draft was accepted')
print('PASS: CIA version limits, packed/TMD versions and rollover; resume rejects changed source, corruption and wrong embedded versions.')
