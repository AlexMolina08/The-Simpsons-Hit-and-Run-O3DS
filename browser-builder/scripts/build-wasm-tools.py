#!/usr/bin/env python3
"""Build browser-only converters and packaging tools from pinned source.

This is a maintainer command. People using the browser builder do not install
Emscripten or run this script. No game data is downloaded or included here.
"""
from __future__ import annotations

import argparse
import ast
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = Path(__file__).resolve().parent
WORK = ROOT / 'out/browser-tools'
OUTPUT = WORK / 'wasm'
EMSCRIPTEN_VERSION = '4.0.23'
SOURCES = {
    'Project_CTR': ('https://github.com/3DSGuy/Project_CTR.git', 'c0488dcb6c3048e6a519716f2b21e2c65859ca75'),
    'tex3ds': ('https://github.com/devkitPro/tex3ds.git', '37220c696e873812df41af62d1db0b20c66661fe'),
    'bannertool': ('https://github.com/carstene1ns/3ds-bannertool.git', '734d33be79fd3f8c29c6296158f06ac7c5ca9dcb'),
}


def run(arguments, *, cwd=ROOT, capture=False, environment=None):
    return subprocess.run([str(part) for part in arguments], cwd=cwd,
                          check=True, capture_output=capture, text=True, env=environment)


def source(name, bootstrap):
    url, revision = SOURCES[name]
    directory = WORK / name
    if not (directory / '.git').is_dir():
        # Source archives produced below already contain the pinned source tree.
        if (directory / ('makerom/src' if name == 'Project_CTR' else 'source')).is_dir():
            return directory
        if not bootstrap:
            raise SystemExit(f'Missing {name}; rerun with --bootstrap to download pinned tool sources')
        run(['git', 'clone', '--no-checkout', '--filter=blob:none', url, directory])
        run(['git', '-C', directory, 'fetch', '--depth', '1', 'origin', revision])
        run(['git', '-C', directory, 'checkout', '--detach', revision])
    actual = run(['git', '-C', directory, 'rev-parse', 'HEAD'], capture=True).stdout.strip()
    if actual != revision:
        raise SystemExit(f'{name} has unexpected revision {actual}; expected {revision}')
    return directory


def generate_texture_converter():
    prepare = ROOT / 'tools/3ds/prepare_native_textures.py'
    output = WORK / 'native_texture_browser.cpp'
    if not prepare.is_file():
        if output.is_file():
            return output
        raise SystemExit('Missing production texture converter source')
    tree = ast.parse(prepare.read_text())
    build = next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == 'build_converter')
    prelude = next(ast.literal_eval(node.value) for node in ast.walk(build)
                   if isinstance(node, ast.Assign) and any(isinstance(target, ast.Name) and target.id == 'prelude'
                                                          for target in node.targets))
    source_text = (ROOT / 'libs/pure3d/pddi/pica/texture.cpp').read_text()

    def function(signature):
        start = source_text.index(signature)
        opening = source_text.index('{', start)
        depth, end = 1, opening + 1
        while depth:
            depth += (source_text[end] == '{') - (source_text[end] == '}')
            end += 1
        return source_text[start:end]

    translation = prelude
    for signature in ('static inline bool IsCompressed', 'void picaTexture::UploadLevel',
                      'void picaTexture::ConvertAndTile'):
        translation += function(signature)
    translation += (SCRIPTS / 'native_texture_adapter.cpp').read_text()
    output.write_text(translation)
    return output


def build(compiler, arguments):
    print(f'Building {arguments[-1]}', flush=True)
    environment = os.environ.copy()
    environment['SOURCE_DATE_EPOCH'] = '0'
    run([compiler, *arguments], environment=environment)


def source_archive():
    """Ship corresponding source for GPL-linked tools, including local adapters."""
    archive_path = OUTPUT / 'wasm-tools-sources.tar.gz'
    def normalized(info):
        info.uid = info.gid = info.mtime = 0
        info.uname = info.gname = ''
        return info

    # Keep this source download reproducible, including tar and gzip metadata.
    with archive_path.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as zipped, \
            tarfile.open(fileobj=zipped, mode='w') as archive:
        for name in SOURCES:
            directory = WORK / name
            wanted = directory / 'makerom' if name == 'Project_CTR' else directory
            for path in sorted(wanted.rglob('*')):
                if not path.is_file() or '.git' in path.parts:
                    continue
                if path.suffix in {'.o', '.a', '.wasm', '.mjs', '.png', '.jpg', '.gif', '.rmv', '.cia'}:
                    continue
                archive.add(path, arcname=path.relative_to(ROOT), filter=normalized)
        for path in sorted(SCRIPTS.glob('*')):
            if path.is_file():
                archive.add(path, arcname=path.relative_to(ROOT), filter=normalized)
        archive.add(WORK / 'native_texture_browser.cpp',
                    arcname='out/browser-tools/native_texture_browser.cpp', filter=normalized)
        for relative in ('libs/pure3d/pddi/pica/texturetile.hpp', 'libs/pure3d/pddi/pica/swizzle.hpp',
                         'libs/pure3d/pddi/pica/swizzle.cpp', 'libs/pure3d/pddi/gl/decompress.h',
                         'libs/pure3d/pddi/gl/decompress.cpp'):
            archive.add(ROOT / relative, arcname=relative, filter=normalized)
        archive.add(ROOT / 'browser-builder/THIRD_PARTY_NOTICES.md',
                    arcname='browser-builder/THIRD_PARTY_NOTICES.md', filter=normalized)
        archive.add(ROOT / 'browser-builder/licenses', arcname='browser-builder/licenses', filter=normalized)
    return archive_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bootstrap', action='store_true', help='Download pinned tool sources and Emscripten when missing')
    parser.add_argument('--emsdk', type=Path, default=WORK / 'emsdk')
    parser.add_argument('--tool', action='append', choices=['etc1', 'native-texture', 'bannertool', 'makerom'])
    arguments = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    OUTPUT.mkdir(exist_ok=True)
    emsdk = arguments.emsdk.resolve()
    em = emsdk / 'upstream/emscripten'
    if not (em / 'emcc').is_file():
        if not arguments.bootstrap:
            parser.error('Emscripten is missing; rerun with --bootstrap')
        if not (emsdk / '.git').is_dir():
            run(['git', 'clone', '--depth', '1', '--branch', EMSCRIPTEN_VERSION,
                 'https://github.com/emscripten-core/emsdk.git', emsdk])
        run([emsdk / 'emsdk', 'install', EMSCRIPTEN_VERSION])
        run([emsdk / 'emsdk', 'activate', EMSCRIPTEN_VERSION])
    compiler_version = run([em / 'emcc', '--version'], capture=True).stdout
    if EMSCRIPTEN_VERSION not in compiler_version.splitlines()[0]:
        parser.error(f'Emscripten {EMSCRIPTEN_VERSION} is required')
    upstream = {name: source(name, arguments.bootstrap) for name in SOURCES}
    # Upstream's variable-length array initializer is rejected by Clang C++17.
    padding = upstream['bannertool'] / 'source/3ds/lz11.cpp'
    original = padding.read_text()
    fixed = original.replace('u8 pad[padLength] = {0};', 'u8 pad[4] = {0};')
    if 'u8 pad[4] = {0};' not in fixed:
        raise SystemExit('Unexpected bannertool padding code; review the portability patch')
    if fixed != original:
        padding.write_text(fixed)
    common = ['-O3', '-DNDEBUG', '-sMODULARIZE=1', '-sEXPORT_ES6=1',
              '-sALLOW_MEMORY_GROWTH=1', '-sENVIRONMENT=web,worker,node', '-sEXIT_RUNTIME=0']
    chosen = set(arguments.tool or ['etc1', 'native-texture', 'bannertool', 'makerom'])
    if 'etc1' in chosen:
        build(em / 'em++', [*common, '-std=c++17', '-I' + str(upstream['tex3ds'] / 'include'),
              SCRIPTS / 'etc1_adapter.cpp', upstream['tex3ds'] / 'source/rg_etc1.cpp',
              '-sEXPORT_NAME=createEtc1', '-sEXPORTED_FUNCTIONS=["_encodeEtc1","_malloc","_free"]',
              '-sEXPORTED_RUNTIME_METHODS=["HEAPU8"]', '--no-entry', '-o', OUTPUT / 'etc1.mjs'])
    if 'native-texture' in chosen:
        translation = generate_texture_converter()
        build(em / 'em++', [*common, '-O2', '-std=c++14', '-DSRR2_OLD3DS_TEXTURE_TILES',
              '-I' + str(ROOT / 'libs/pure3d'), translation,
              ROOT / 'libs/pure3d/pddi/pica/swizzle.cpp', ROOT / 'libs/pure3d/pddi/gl/decompress.cpp',
              '-sEXPORT_NAME=createNativeTexture',
              '-sEXPORTED_FUNCTIONS=["_convert_texture","_malloc","_free"]',
              '-sEXPORTED_RUNTIME_METHODS=["HEAPU8"]', '--no-entry', '-o', OUTPUT / 'native-texture.mjs'])
    filesystem = ['-sFORCE_FILESYSTEM=1', '-sINVOKE_RUN=0',
                  '-sEXPORTED_FUNCTIONS=["_main","_malloc","_free"]',
                  '-sEXPORTED_RUNTIME_METHODS=["FS","callMain","WORKERFS","NODEFS","HEAPU8"]',
                  '-lworkerfs.js', '-lnodefs.js']
    if 'bannertool' in chosen:
        folder = upstream['bannertool'] / 'source'
        build(em / 'em++', [*common, *filesystem, '-std=c++17', '-DVERSION="1.2.3"',
              '-DSTBI_ONLY_PNG', '-DSTBI_NO_LINEAR', '-DSTBI_NO_STDIO',
              '-DSTB_VORBIS_NO_PUSHDATA_API', '-DSTB_VORBIS_NO_STDIO', '-DDR_WAV_NO_STDIO',
              '-I' + str(folder), '-I' + str(folder / 'pc'),
              *sorted(folder.glob('*.cpp')), *sorted((folder / '3ds').glob('*.cpp')),
              *sorted((folder / 'pc').glob('*.cpp')), '-sEXPORT_NAME=createBannertool',
              '-o', OUTPUT / 'bannertool.mjs'])
    if 'makerom' in chosen:
        folder = upstream['Project_CTR'] / 'makerom'
        dependencies = [folder / 'deps' / name for name in ('libblz', 'libmbedtls', 'libyaml')]
        build(em / 'emcc', [*common, *filesystem, '-std=c11', '-D_GNU_SOURCE',
              *['-I' + str(dep / 'include') for dep in dependencies], *sorted((folder / 'src').glob('*.c')),
              *[path for dep in dependencies for path in sorted((dep / 'src').glob('*.c'))],
              '-sEXPORT_NAME=createMakerom', '-sINITIAL_MEMORY=67108864',
              '-sMAXIMUM_MEMORY=4294967296', '-o', OUTPUT / 'makerom.mjs'])
    generate_texture_converter()
    archive = source_archive()
    manifest = {'emscripten': EMSCRIPTEN_VERSION,
                'sources': {name: {'url': url, 'revision': revision} for name, (url, revision) in SOURCES.items()},
                'files': {path.name: {'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                          for path in sorted(OUTPUT.glob('*')) if path.suffix in {'.mjs', '.wasm'}},
                'corresponding_source': {'file': archive.name, 'bytes': archive.stat().st_size,
                                         'sha256': hashlib.sha256(archive.read_bytes()).hexdigest()}}
    (OUTPUT / 'tool-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Browser tools and corresponding source: {OUTPUT}')


if __name__ == '__main__':
    main()
