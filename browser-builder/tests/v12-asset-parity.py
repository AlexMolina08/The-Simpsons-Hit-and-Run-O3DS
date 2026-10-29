#!/usr/bin/env python3
"""Opt-in accepted-asset parity using private local PC files; no game data is shipped."""
import argparse
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/3ds'))
from prepare_streaming_order import RELEASE_FILES, release_bytes
spec = importlib.util.spec_from_file_location('v12_assets', ROOT/'browser-builder/engine-conversion/v12-assets.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--accepted', type=Path, required=True)
    parser.add_argument('--native-helper', type=Path)
    parser.add_argument('--wasm', type=Path)
    parser.add_argument('--rounding', type=Path, default=ROOT/'browser-builder/engine-conversion/v12-rounding.json')
    parser.add_argument('--stage', action='append', choices=['cars', 'characters', 'hud', 'world', 'landmarks'])
    parser.add_argument('--levels', default='1234567')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if not args.native_helper and not args.wasm:
        parser.error('Pass a native helper or the built WASM module.')
    original_run = subprocess.run
    bridge = subprocess.Popen(['node', str(ROOT/'browser-builder/tests/v12-occlusion.mjs'), str(args.wasm.resolve()), '--bridge'],
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE) if args.wasm else None

    def occlusion(request, sun):
        if bridge:
            bridge.stdin.write(struct.pack('<II3f', len(request), bool(sun), *(sun or (0, 1, 0))))
            bridge.stdin.write(request)
            bridge.stdin.flush()
            size = bridge.stdout.read(4)
            if len(size) != 4:
                raise RuntimeError('WASM parity bridge stopped.')
            result = bridge.stdout.read(struct.unpack('<I', size)[0])
        else:
            command = [str(args.native_helper.resolve()), *(['--sun', *(str(value) for value in sun)] if sun else [])]
            result = original_run(command, input=request, capture_output=True, check=True).stdout
        return result

    results = []
    try:
        with tempfile.TemporaryDirectory(prefix='srr-v12-asset-parity-') as folder:
            for stage in args.stage or ['cars', 'characters', 'hud', 'world', 'landmarks']:
                for level in args.levels if stage == 'world' else [None]:
                    start = time.perf_counter()
                    output = Path(folder)/(stage+(level or ''))
                    rounding = args.rounding.read_text() if args.rounding.is_file() else None
                    result = assets.prepare_stage(stage, args.game, output, level=level, occlusion=occlusion,
                                                  rounding=rounding)
                    mismatches = []
                    for item in result['files']:
                        path = output/item['path']
                        accepted = args.accepted/item['path']
                        expected = accepted.read_bytes() if accepted.is_file() else None
                        if expected is not None and path.name in RELEASE_FILES:
                            expected = release_bytes(path.name, expected,
                                (args.accepted/'art/l2_TERRA.p3d').read_bytes())
                        if expected is None or path.read_bytes() != expected:
                            mismatches.append(item['path'])
                    record = {'stage': stage, 'level': level, 'seconds': time.perf_counter()-start,
                              'files': len(result['files']), 'mismatches': mismatches,
                              'ray_backend': 'wasm' if bridge else 'native'}
                    results.append(record)
                    print(json.dumps(record), flush=True)
                    if args.report:
                        args.report.write_text(json.dumps(results, indent=2)+'\n')
                    if mismatches:
                        raise AssertionError(f'Accepted assets differ: {mismatches}')
        assert subprocess.run is original_run, 'Helper interception leaked beyond preparation.'
    finally:
        if bridge:
            bridge.stdin.close()
            if bridge.wait(timeout=30) != 0:
                raise RuntimeError('WASM parity bridge failed.')


if __name__ == '__main__':
    main()
