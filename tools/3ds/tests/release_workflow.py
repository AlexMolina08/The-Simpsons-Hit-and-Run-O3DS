#!/usr/bin/env python3
"""Check CIA version limits and the integrity gates used by resumed uploads."""
import hashlib
import json
from pathlib import Path
import sys
import struct
import tempfile
from types import SimpleNamespace
from unittest.mock import patch
from contextlib import redirect_stderr, redirect_stdout
import io

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from release_version import parse, next_version, packed_version, read_cia_version
from release_cia import verify_artifact, wait_for_release, main as release_main

publication_attempts = [[], ['--version', '1.1.1'], ['--version', '1.1.1', '--resume'], ['--dry-run']]
for flag in ('--stable', '--resume', '--dry-run'):
    publication_attempts.append(['--version', '1.1.1', flag])
for flag in ('--repo', '--data', '--movies', '--game', '--makerom', '--bannertool', '--notes-file'):
    publication_attempts.append(['--version', '1.1.1', flag, 'owner/repository' if flag == '--repo' else '/tmp/unused'])
for arguments in publication_attempts:
    with patch('sys.argv', ['release_cia.py', *arguments]), patch('release_cia.run') as command, redirect_stderr(io.StringIO()) as error:
        try:
            release_main()
        except SystemExit as stopped:
            assert stopped.code == 2
        else:
            raise AssertionError('Complete game-data publication was not blocked')
        assert 'Public uploads of complete game-data CIAs are disabled' in error.getvalue()
        command.assert_not_called()

def private_git(*arguments):
    values = {('branch', '--show-current'): 'main', ('rev-parse', 'HEAD'): 'a' * 40,
              ('status', '--porcelain'): '', ('config', 'user.name'): 'Local Builder',
              ('config', 'user.email'): 'builder@example.invalid', ('tag', '--list', 'v*'): ''}
    return values[arguments]

for extra in ([], ['--stable'], ['--resume']):
    with patch('sys.argv', ['release_cia.py', '--no-upload', '--dry-run', '--version', '1.2.3', *extra]), \
            patch('release_cia.git', side_effect=private_git), patch('release_cia.run') as command, \
            redirect_stdout(io.StringIO()) as output:
        release_main()
        assert 'Public uploads are disabled' in output.getvalue()
        command.assert_not_called()

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
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    output = root / 'out/releases/v1.2.3'
    output.mkdir(parents=True)
    cia = output / 'Simpsons-Hit-and-Run-3DS-v1.2.3.cia'
    cia.write_bytes(cia_fixture(1059))
    cia.with_suffix('.manifest.json').write_text(json.dumps({
        'source_commit': 'a' * 40, 'version': '1.2.3', 'cia_bytes': cia.stat().st_size,
        'cia_sha256': hashlib.sha256(cia.read_bytes()).hexdigest()}))
    with patch('sys.argv', ['release_cia.py', '--no-upload', '--resume', '--version', '1.2.3']), \
            patch('release_cia.REPO', root), patch('release_cia.git', side_effect=private_git), \
            patch('release_cia.run', return_value=SimpleNamespace(returncode=0)) as command, \
            redirect_stdout(io.StringIO()) as output_log:
        release_main()
        assert 'Verified local CIA' in output_log.getvalue()
        assert command.call_count == 1
        assert command.call_args.args[0] == ['git', 'merge-base', '--is-ancestor', 'a' * 40, 'a' * 40]
print('PASS: public CIA upload attempts stop before commands; local dry runs avoid gh; CIA version limits and private artifact-integrity gates pass.')
