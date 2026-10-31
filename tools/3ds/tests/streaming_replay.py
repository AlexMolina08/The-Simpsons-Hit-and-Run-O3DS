#!/usr/bin/env python3
"""A repeat must retain exact artifacts and account for every experiment override."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile

script = Path(__file__).resolve().parents[1] / 'replay_streaming_run.py'
spec = importlib.util.spec_from_file_location('streaming_replay', script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as directory:
    p = Path(directory)
    artifact, elf = p / 'test.3dsx', p / 'test.elf'
    artifact.write_bytes(b'recorded binary')
    elf.write_bytes(b'recorded symbols')
    record = dict(artifact=str(artifact), elf=str(elf), host='127.0.0.1', port=80,
        sha256=hashlib.sha256(artifact.read_bytes()).hexdigest(),
        elf_sha256=hashlib.sha256(elf.read_bytes()).hexdigest(), trial='reference',
        remote_path='/3ds/hitrun-dev/candidate-000000000001.3dsx',
        arguments=['graphics_stereo=0', 'bench_idle_loader=0', 'bench_trial=reference', 'dev_receiver=1'])
    manifest = p / 'manifest.json'
    manifest.write_text(json.dumps(record))
    command, evidence = module.prepare(manifest, 'candidate', p / 'output', ['bench_idle_loader=1'], True)
    assert 'graphics_stereo=0' in command and 'bench_idle_loader=1' in command
    assert 'bench_idle_loader=0' not in command and 'bench_trial=reference' not in command
    assert 'bench_trial=candidate' in command and '--reuse-remote' in command
    assert set(evidence['changes']) == {'bench_idle_loader'}
    assert not (p / 'output').exists()
    for overrides in (['bench_trial=wrong'], ['bench_idle_loader=1', 'bench_idle_loader=0'], ['bad\nargument=1']):
        try:
            module.prepare(manifest, 'candidate', p / 'output', overrides)
            raise AssertionError('Unsafe or ambiguous override accepted')
        except ValueError:
            pass
    artifact.write_bytes(b'replaced binary')
    try:
        module.prepare(manifest, 'candidate', p / 'output', [])
        raise AssertionError('Changed binary accepted')
    except ValueError:
        pass
print('PASS: replay verifies both artifacts, preserves conditions, records explicit changes and refuses ambiguous overrides or replaced binaries.')
