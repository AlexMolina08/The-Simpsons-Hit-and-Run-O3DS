#!/usr/bin/env python3
"""Build the small production DSP encoder for browser asset preparation."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
VERSION = '4.0.23'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(emsdk, output):
    compiler = emsdk / 'upstream/emscripten/em++'
    version = subprocess.check_output([str(compiler), '--version'], text=True)
    if VERSION not in version.splitlines()[0]:
        raise ValueError(f'Emscripten {VERSION} is required')
    output.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'libs/radsound/src/hal/ctr/ndsphal.cpp'
    text = source.read_text()
    start = text.index('static u16 s_AdpcmCoefs')
    end = text.index('// Per-sample pruning regressed', start)
    kernel = output / 'audio_cache_kernel.inc'
    kernel.write_text(text[start:end])
    adapter = Path(__file__).with_name('audio_cache_adapter.cpp')
    environment = os.environ.copy()
    environment['SOURCE_DATE_EPOCH'] = '0'
    subprocess.run([str(compiler), '-std=c++17', '-O3', '-DNDEBUG', str(adapter),
                    '-I' + str(output), '-sMODULARIZE=1', '-sEXPORT_ES6=1',
                    '-sENVIRONMENT=web,worker,node', '-sALLOW_MEMORY_GROWTH=1',
                    '-sINITIAL_MEMORY=16777216', '-sMAXIMUM_MEMORY=33554432',
                    '-sEXPORT_NAME=createAudioCacheEncoder',
                    '-sEXPORTED_FUNCTIONS=["_encode_audio_cache","_malloc","_free"]',
                    '-sEXPORTED_RUNTIME_METHODS=["HEAPU8"]', '--no-entry',
                    '-o', str(output / 'v12-audio.mjs')], check=True, env=environment)
    archive = output / 'v12-audio-sources.tar.gz'
    def normalized(info):
        info.uid = info.gid = info.mtime = 0
        info.uname = info.gname = ''
        return info
    with archive.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as zipped, \
            tarfile.open(fileobj=zipped, mode='w') as bundle:
        for path, name in ((source, str(source.relative_to(ROOT))),
                           (adapter, str(adapter.relative_to(ROOT))),
                           (Path(__file__), str(Path(__file__).relative_to(ROOT))),
                           (kernel, 'audio_cache_kernel.inc')):
            bundle.add(path, arcname=name, filter=normalized)
    manifest = {'emscripten': VERSION, 'productionSource': str(source.relative_to(ROOT)),
                'productionSourceSHA256': digest(source), 'kernelSHA256': digest(kernel),
                'predictors': 3, 'maximumClipSamples': 441000,
                'files': {name: {'bytes': (output/name).stat().st_size, 'sha256': digest(output/name)}
                          for name in ('v12-audio.mjs', 'v12-audio.wasm', archive.name)}}
    (output/'v12-audio-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emsdk', type=Path, default=ROOT/'out/browser-tools/emsdk')
    parser.add_argument('--out', type=Path, default=ROOT/'out/browser-tools/v12-audio')
    args = parser.parse_args()
    build(args.emsdk.resolve(), args.out.resolve())
