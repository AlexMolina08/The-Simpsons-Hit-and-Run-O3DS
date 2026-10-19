#!/usr/bin/env python3
"""Check CIA version limits and the integrity gates used by resumed uploads."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from release_version import parse, next_version
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
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    cia, manifest = root / 'game.cia', root / 'game.manifest.json'
    cia.write_bytes(b'owned release fixture')
    metadata = {'source_commit': 'source-one', 'cia_bytes': cia.stat().st_size,
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
with patch('release_cia.release_info', side_effect=[None, None, {'assets': []}]), patch('release_cia.time.sleep'):
    assert wait_for_release('owner/repo', 'v1.2.3') == {'assets': []}
with patch('release_cia.release_info', return_value=None), patch('release_cia.time.sleep'):
    try:
        wait_for_release('owner/repo', 'v1.2.3')
    except RuntimeError:
        pass
    else:
        raise AssertionError('An invisible draft was accepted')
print('PASS: CIA version limits and rollover; resumed uploads reject changed source and corrupted artifacts.')
