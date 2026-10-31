#!/usr/bin/env python3
"""Exercise acceptance rejection against a recovered physical run, without changing it."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from summarize_world_stress import analyze

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--run', type=Path, required=True)
parser.add_argument('--catalog', type=Path, required=True)
args = parser.parse_args()
assert analyze(args.run, args.catalog)['runtime_checks_passed']
with tempfile.TemporaryDirectory(prefix='stress-summary-test-') as directory:
    work = Path(directory)
    shutil.copytree(args.run/'console-logs', work/'console-logs')
    manifest = json.loads((args.run/'manifest.json').read_text())
    manifest.pop('manual_recovery', None)
    manifest['complete'] = True
    path = work/'manifest.json'
    path.write_text(json.dumps(manifest))
    native = work/'console-logs/native-graphics-modes.log'
    boot = work/'console-logs/old3ds.log'
    original_native, original_boot = native.read_text(), boot.read_text()
    assert analyze(work, args.catalog)['passed']

    def reject(extra_native='', extra_boot=''):
        native.write_text(original_native+extra_native)
        boot.write_text(original_boot+extra_boot)
        result = analyze(work, args.catalog)
        assert not result['passed'] and not result['runtime_checks_passed'], result
        native.write_text(original_native)
        boot.write_text(original_boot)

    reject('\nWORLD_ZONE_QUEUE capacity=32 peak=33 failures=0\n')
    reject('\nWORLD_ZONE_QUEUE capacity=32 peak=1 failures=1\n')
    reject('\nWORLD_RESIDENCY_SET opened=1 published=1 admitted=00 rejected=00\n')
    reject('\nNATIVE_MEMORY_CAPACITY normal_allocator_bytes='+str(manifest['preflight']['normal_heap'])+
           ' excludes_main_stack_and_tls=1\n')
    reject('\nNATIVE_MEMORY diagnostic=1 normal_failures=0 linear_failures=0 linear_free_min='+
           str(manifest['preflight']['linear_heap']+1)+'\n')
    reject(extra_boot='\nNATIVE_WRAPPER_INTEGRITY failures=1\n')
    if any(argument.startswith('bench_seconds=') and int(argument.split('=')[1]) >= 60
           for argument in manifest['arguments']):
        reject('\nNATIVE_ROUTE road=1 input_only=1 distance=0\n')
        native.write_text('\n'.join(re.sub(r'\bspeed=[^ ]+', 'speed=0', line)
                                    if line.startswith('NATIVE_FRAME ') else line
                                    for line in original_native.splitlines()))
        assert not analyze(work, args.catalog)['runtime_checks_passed']
        native.write_text(original_native)

    # Never turn a manually recovered run into automatic transport acceptance.
    manifest['complete'] = False
    path.write_text(json.dumps(manifest))
    result = analyze(work, args.catalog)
    assert result['runtime_checks_passed'] and not result['transport_complete'] and not result['passed']
    manifest['complete'] = True
    manifest['manual_recovery'] = {'receiver_reopened_by_user': True}
    path.write_text(json.dumps(manifest))
    result = analyze(work, args.catalog)
    assert result['runtime_checks_passed'] and not result['transport_complete'] and not result['passed']
    manifest.pop('manual_recovery')

    # An instrumented binary must not pass after its required records disappear.
    binary = work/'instrumented.3dsx'
    binary.write_bytes(Path(manifest['artifact']).read_bytes()+b'WORLD_ZONE_QUEUE\0WORLD_RESIDENCY_SET opened=\0')
    manifest.update(artifact=str(binary), sha256=hashlib.sha256(binary.read_bytes()).hexdigest(), complete=True)
    path.write_text(json.dumps(manifest))
    native.write_text('\n'.join(line for line in original_native.splitlines()
                                if not line.startswith(('WORLD_ZONE_QUEUE ', 'WORLD_RESIDENCY_SET '))))
    result = analyze(work, args.catalog)
    assert not result['runtime_checks_passed'] and not result['passed']

print('PASS: queue overflow/allocation, missing diagnostics, absent admission, loader corruption, inflated memory capacity and manual transport recovery cannot pass.')
