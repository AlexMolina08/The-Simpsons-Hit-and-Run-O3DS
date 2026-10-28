#!/usr/bin/env python3
"""Build the isolated, decompress-only browser FreeArc codec from pinned source."""
from pathlib import Path
import argparse
import gzip
import hashlib
import json
import os
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'out/browser-tools/iso-extraction'
SOURCE = WORK / 'freearc-067-source'
OUTPUT = WORK / 'wasm'
URL = 'https://github.com/mirror/freearc.git'
REVISION = '71f3ab36df26401fff4301b4c8600a31a90d8da9'
SOURCES = ['Environment.cpp', 'Compression/Common.cpp', 'Compression/CompressionLibrary.cpp',
    'Compression/MultiThreading.cpp', 'Compression/LZMA2/C_LZMA.cpp', 'Compression/LZMA2/C_BCJ.cpp',
    'Compression/Delta/C_Delta.cpp', 'Compression/REP/C_REP.cpp', 'Compression/GRZip/C_GRZip.cpp',
    'Compression/Tornado/C_Tornado.cpp', 'Compression/4x4/C_4x4.cpp',
    'Compression/_Encryption/C_Encryption.cpp']


def run(args, **kwargs):
    subprocess.run([str(value) for value in args], check=True, **kwargs)


def source_archive():
    destination = OUTPUT / 'freearc-sources.tar.gz'
    def normalized(info):
        info.uid = info.gid = info.mtime = 0
        info.uname = info.gname = ''
        return info
    with destination.open('wb') as raw, gzip.GzipFile(filename='', fileobj=raw, mode='wb', mtime=0) as zipped, \
            tarfile.open(fileobj=zipped, mode='w') as archive:
        for path in sorted(SOURCE.rglob('*')):
            if not path.is_file() or '.git' in path.parts:
                continue
            if path.suffix.lower() in {'.exe', '.dll', '.o', '.a', '.lib', '.zip', '.7z', '.png', '.jpg', '.bmp'}:
                continue
            archive.add(path, arcname=path.relative_to(ROOT), filter=normalized)
        for path in [Path(__file__).resolve(), ROOT / 'browser-builder/scripts/freearc_codec.cpp',
                     ROOT / 'browser-builder/engine-conversion/repack.mjs', ROOT / 'browser-builder/opfs.mjs',
                     ROOT / 'browser-builder/THIRD_PARTY_NOTICES.md', ROOT / 'browser-builder/licenses']:
            archive.add(path, arcname=path.relative_to(ROOT), filter=normalized)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bootstrap', action='store_true')
    parser.add_argument('--emsdk', type=Path, default=ROOT / 'out/browser-tools/emsdk')
    options = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    OUTPUT.mkdir(exist_ok=True)
    if (SOURCE / '.git').is_dir():
        actual = subprocess.check_output(['git', '-C', str(SOURCE), 'rev-parse', 'HEAD'], text=True).strip()
        if actual != REVISION:
            raise SystemExit('The FreeArc source revision does not match the pinned release')
    elif not (SOURCE / 'Compression/CompressionLibrary.cpp').exists():
        if not options.bootstrap:
            raise SystemExit('Missing FreeArc source; rerun with --bootstrap')
        run(['git', 'clone', '--no-checkout', '--filter=blob:none', URL, SOURCE])
        run(['git', '-C', SOURCE, 'fetch', '--depth', '1', 'origin', REVISION])
        run(['git', '-C', SOURCE, 'checkout', '--detach', REVISION])
    # The upstream GRZip pointer table assumes four-byte pointers. This keeps
    # its allocation correct in native 64-bit validation too; WASM32 is unchanged.
    lzp = SOURCE / 'Compression/GRZip/LZP.c'
    content = lzp.read_bytes().replace(b'(LZP_HT_Size+1)*sizeof(uint32)',
                                      b'(LZP_HT_Size+1)*sizeof(*Contexts)')
    lzp.write_bytes(content)
    # The original Unix worker stack is too small on macOS for native archive
    # verification. Keep the same portable source adjustment in this archive;
    # browser decoding uses the sequential adapter instead of worker threads.
    threads = SOURCE / 'Compression/LZMA2/C/ThreadsUnix.c'
    content = threads.read_bytes()
    if b'pthread_attr_setstacksize(&attr, 8*1024*1024);' not in content:
        content = content.replace(b'pthread_attr_init(&attr);',
            b'pthread_attr_init(&attr);\n  pthread_attr_setstacksize(&attr, 8*1024*1024);')
        threads.write_bytes(content)
    em = options.emsdk.resolve() / 'upstream/emscripten'
    version = subprocess.check_output([str(em / 'em++'), '--version'], text=True).splitlines()[0]
    if '4.0.23' not in version:
        raise SystemExit('Emscripten 4.0.23 is required')
    flags = ['-std=gnu++14', '-O2', '-w', '-fno-strict-aliasing', '-fno-exceptions', '-fno-rtti',
        '-ffile-prefix-map=' + str(ROOT) + '=.', '-DFREEARC_UNIX', '-DFREEARC_INTEL_BYTE_ORDER',
        '-DFREEARC_DECOMPRESS_ONLY', '-D_NO_EXCEPTIONS', '-DUNARC', '-Wno-c++11-narrowing',
        '-DCPU_Is_Aes_Supported()=0', '-I' + str(SOURCE), '-I' + str(SOURCE / 'Compression/_Encryption/headers'),
        '-sMODULARIZE=1', '-sEXPORT_ES6=1', '-sEXPORT_NAME=createFreeArcCodec',
        '-sENVIRONMENT=web,worker,node', '-sALLOW_MEMORY_GROWTH=1', '-sINITIAL_MEMORY=67108864',
        '-sMAXIMUM_MEMORY=2147483648', '-sSTACK_SIZE=8388608', '-sFORCE_FILESYSTEM=1',
        '-sINVOKE_RUN=0', '-sEXIT_RUNTIME=0', '-sEXPORTED_FUNCTIONS=["_main","_malloc","_free"]',
        '-sEXPORTED_RUNTIME_METHODS=["FS","callMain","WORKERFS","NODEFS","HEAPU8"]',
        '-lworkerfs.js', '-lnodefs.js']
    environment = os.environ.copy()
    environment['SOURCE_DATE_EPOCH'] = '0'
    run([em / 'em++', *flags, ROOT / 'browser-builder/scripts/freearc_codec.cpp',
         *[SOURCE / name for name in SOURCES], '-o', OUTPUT / 'freearc-codec.mjs'], env=environment)
    archive = source_archive()
    paths = [OUTPUT / 'freearc-codec.mjs', OUTPUT / 'freearc-codec.wasm', archive]
    manifest = {'emscripten': '4.0.23', 'source': {'url': URL, 'revision': REVISION},
        'purpose': 'Decompression of FreeArc game-data archives only; installer code is never executed.',
        'changes': ['Sequential GRZip and 4x4 scheduling adapter', 'GRZip pointer allocation portability',
                    'Portable C AES implementation and normalized source paths'],
        'files': {path.name: {'bytes': path.stat().st_size,
                            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for path in paths}}
    (OUTPUT / 'freearc-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'FreeArc codec and corresponding source: {OUTPUT}')


if __name__ == '__main__':
    main()
