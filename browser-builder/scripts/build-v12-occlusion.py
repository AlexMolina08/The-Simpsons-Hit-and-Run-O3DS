#!/usr/bin/env python3
"""Build the existing v1.2 ray baker for browser workers; maintainers only."""
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emsdk', type=Path, default=ROOT/'out/browser-tools/emsdk')
    parser.add_argument('--output', type=Path, default=ROOT/'out/browser-tools/v12-occlusion')
    args = parser.parse_args()
    compiler = args.emsdk.resolve()/'upstream/emscripten/em++'
    version = subprocess.check_output([str(compiler), '--version'], text=True).splitlines()[0]
    if VERSION not in version:
        parser.error(f'Emscripten {VERSION} is required; found {version}')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    adapter = ROOT/'browser-builder/scripts/occlusion_adapter.cpp'
    flags = ['-O3', '-std=c++17', '-ffp-contract=off', '-fno-fast-math',
             '-sMODULARIZE=1', '-sEXPORT_ES6=1', '-sEXPORT_NAME=createOcclusion',
             '-sALLOW_MEMORY_GROWTH=1', '-sINITIAL_MEMORY=33554432',
             '-sMAXIMUM_MEMORY=1073741824', '-sENVIRONMENT=web,worker,node',
             '-sFILESYSTEM=0', '-sEXIT_RUNTIME=0',
             '-sEXPORTED_FUNCTIONS=["_v12_occlusion_create","_v12_occlusion_count",'
             '"_v12_occlusion_sample","_v12_occlusion_destroy","_v12_occlusion_bake","_malloc","_free"]',
             '-sEXPORTED_RUNTIME_METHODS=["HEAPU8"]', '--no-entry']
    environment = dict(os.environ, SOURCE_DATE_EPOCH='0')
    subprocess.run([str(compiler), *flags, str(adapter), '-o', str(output/'occlusion.mjs')],
                   env=environment, check=True)
    sources = [adapter, Path(__file__).resolve(), ROOT/'tools/3ds/character_occlusion.cpp']
    archive = output/'occlusion-sources.tar.gz'
    with archive.open('wb') as stream, gzip.GzipFile(fileobj=stream, mode='wb', mtime=0) as zipped:
        with tarfile.open(fileobj=zipped, mode='w') as tar:
            for source in sources:
                info = tar.gettarinfo(str(source), str(source.relative_to(ROOT)))
                info.uid = info.gid = info.mtime = 0
                info.uname = info.gname = ''
                with source.open('rb') as payload:
                    tar.addfile(info, payload)
    files = {name: {'sha256': digest(output/name), 'bytes': (output/name).stat().st_size}
             for name in ('occlusion.mjs', 'occlusion.wasm', archive.name)}
    manifest = {'emscripten': VERSION, 'compiler': version, 'flags': flags,
                'production_source_sha256': digest(ROOT/'tools/3ds/character_occlusion.cpp'),
                'adapter_sha256': digest(adapter), 'files': files,
                'game_data_included': False}
    (output/'v12-occlusion-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print(json.dumps({'output': str(output), 'files': files}, indent=2))


if __name__ == '__main__':
    main()
