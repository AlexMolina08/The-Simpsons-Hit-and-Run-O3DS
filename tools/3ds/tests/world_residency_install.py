#!/usr/bin/env python3
"""Check generated-subtree ownership, repair and publication rollback."""
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import install_world_residency as installer


def snapshot(root):
    return {path.relative_to(root).as_posix(): path.read_bytes()
            for path in root.rglob('*') if path.is_file()}


with tempfile.TemporaryDirectory(prefix='world-install-test-') as directory:
    root = Path(directory)
    data = root/'game'
    (data/'art').mkdir(parents=True)
    (data/'art/original.p3d').write_bytes(b'original scenery')
    (data/'Save1').write_bytes(b'personal save')
    bundle = data/'native146.bundle'
    bundle.write_bytes(b'native texture bundle')
    fixture = root/'fixture'
    (fixture/'art').mkdir(parents=True)
    (fixture/'art/prepared.p3d').write_bytes(b'prepared scenery')
    (fixture/'asset-admission.bin').write_bytes(b'admission marker')
    report = {'format': 1, 'generated_by': installer.GENERATOR,
              'files': {path.relative_to(fixture).as_posix():
                        {'bytes': path.stat().st_size,
                         'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                        for path in fixture.rglob('*') if path.is_file()}}
    (fixture/'manifest.json').write_text(json.dumps(report))
    originals = snapshot(data)

    def prepare(art, overlay, output, *, native_bundle_sha256):
        assert art == data/'art' and overlay is None
        assert native_bundle_sha256 == hashlib.sha256(bundle.read_bytes()).hexdigest()
        shutil.copytree(fixture, output)
        return report

    with patch.object(installer, 'prepare_all', side_effect=prepare):
        assert installer.install(data, bundle)['changed']
        target = data/'world-residency'
        assert snapshot(target) == snapshot(fixture)
        assert not installer.install(data, bundle)['changed']
        (target/'art/prepared.p3d').write_bytes(b'corrupt')
        assert installer.install(data, bundle)['changed']
        assert snapshot(target) == snapshot(fixture)

        (target/'user-notes.txt').write_bytes(b'preserve user file')
        before = snapshot(target)
        try:
            installer.install(data, bundle)
            raise AssertionError('Unowned file was replaced')
        except ValueError:
            pass
        assert snapshot(target) == before
        (target/'user-notes.txt').unlink()

        (target/'art/prepared.p3d').write_bytes(b'old generated payload')
        before = snapshot(target)
        rename = installer.os.rename

        def fail_publication(source, destination):
            if Path(source).name == 'prepared':
                raise OSError('injected publication failure')
            return rename(source, destination)

        with patch.object(installer.os, 'rename', side_effect=fail_publication):
            try:
                installer.install(data, bundle)
                raise AssertionError('Injected publication failure was ignored')
            except OSError:
                pass
        assert snapshot(target) == before
        assert not list(data.glob('.world-residency-*'))
        assert {name: (data/name).read_bytes() for name in originals} == originals

print('PASS: complete publication, idempotent reuse, corruption repair, unowned-file preservation, rollback and original saves/artwork.')
