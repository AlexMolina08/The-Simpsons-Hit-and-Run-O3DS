#!/usr/bin/env python3
"""Build the small browser media converter and its complete source download.

Maintainer command; visitors do not install a helper or compiler. This build uses
only FFmpeg's own codecs/filters and zlib, without x264, x265, network protocols,
or the other external codecs in the general-purpose npm FFmpeg core.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'out/browser-tools'
DESTINATION = WORK / 'minimal-ffmpeg'
EMSCRIPTEN = '4.0.23'
SOURCES = {
    'FFmpeg-5.1.4': ('https://github.com/FFmpeg/FFmpeg.git', '4729204c17f756e186d622060088371d10b34f7e'),
    'ffmpeg.wasm': ('https://github.com/ffmpegwasm/ffmpeg.wasm.git', '71aa99d37c02a7b4c435275ca9ef50e612f6efa1'),
}


def run(arguments, *, cwd=ROOT, environment=None, log=None):
    subprocess.run([str(value) for value in arguments], cwd=cwd, env=environment,
                   stdout=log, stderr=subprocess.STDOUT if log else None, check=True)


def source(name, bootstrap):
    directory = WORK / name
    url, revision = SOURCES[name]
    if (directory / '.git').is_dir():
        actual = subprocess.check_output(['git', '-C', str(directory), 'rev-parse', 'HEAD'], text=True).strip()
        if actual != revision:
            raise SystemExit(f'{name} revision {actual} does not match {revision}')
    elif not (directory / ('configure' if name == 'FFmpeg-5.1.4' else 'Dockerfile')).exists():
        if not bootstrap:
            raise SystemExit(f'Missing {name}; use --bootstrap to fetch the pinned source')
        run(['git', 'clone', '--no-checkout', '--filter=blob:none', url, directory])
        run(['git', '-C', directory, 'fetch', '--depth', '1', 'origin', revision])
        run(['git', '-C', directory, 'checkout', '--detach', revision])
    return directory


def archive_sources(ffmpeg, integration, emsdk):
    archive = DESTINATION / 'ffmpeg-sources.tar.gz'

    def normalized(info):
        info.uid = info.gid = info.mtime = 0
        info.uname = info.gname = ''
        return info

    with archive.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as compressed, \
            tarfile.open(fileobj=compressed, mode='w') as tar:
        directories = [ffmpeg, integration / 'src/bind', integration / 'src/fftools', integration / 'build',
                       emsdk / 'upstream/emscripten/cache/ports/zlib/zlib-1.3.1']
        for directory in directories:
            for path in sorted(directory.rglob('*')):
                if path.is_file() and '.git' not in path.parts:
                    if directory.name == 'zlib-1.3.1':
                        name = Path('out/browser-tools/zlib-1.3.1') / path.relative_to(directory)
                    else:
                        name = path.relative_to(ROOT)
                    tar.add(path, arcname=name, filter=normalized)
        for path in [integration / 'Dockerfile', integration / 'LICENSE', Path(__file__).resolve(),
                     ROOT / 'browser-builder/THIRD_PARTY_NOTICES.md', ROOT / 'browser-builder/licenses']:
            tar.add(path, arcname=path.relative_to(ROOT), filter=normalized)
    return archive


def patch_xbox_adpcm(ffmpeg):
    # FFmpeg 8 has a separate Xbox decoder which drops the last sample of each
    # 65-sample decoded block. FFmpeg 5.1 maps Xbox WAV tag 0x0069 to IMA WAV.
    # Apply the same sample-count rule only to that Xbox tag; normal WAV stays
    # unchanged. Source: FFmpeg n8.0 libavcodec/adpcm.c, ADPCM_IMA_XBOX case.
    source = ffmpeg / 'libavcodec/adpcm.c'
    text = source.read_text()
    marker = '/* Xbox IMA blocks contain 64 samples; the final decoded sample is padding. */'
    if marker not in text:
        needle = '        ) /* End of CASE */\n    CASE(ADPCM_4XM,'
        replacement = ('        ' + marker + '\n'
            '        if (avctx->codec_tag == 0x0069 && avctx->bits_per_coded_sample == 4)\n'
            '            frame->nb_samples--;\n' + needle)
        if text.count(needle) != 1:
            raise SystemExit('Unexpected FFmpeg IMA WAV decode source; review Xbox sample-count patch')
        source.write_text(text.replace(needle, replacement))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bootstrap', action='store_true')
    parser.add_argument('--emsdk', type=Path, default=WORK / 'emsdk')
    parser.add_argument('--jobs', type=int, default=6)
    parser.add_argument('--skip-libraries', action='store_true', help='Relink already configured/built libraries')
    arguments = parser.parse_args()
    emsdk = arguments.emsdk.resolve()
    em = emsdk / 'upstream/emscripten'
    if not (em / 'emcc').exists():
        raise SystemExit('First run build-wasm-tools.py --bootstrap to prepare Emscripten 4.0.23')
    version = subprocess.check_output([str(em / 'emcc'), '--version'], text=True).splitlines()[0]
    if EMSCRIPTEN not in version:
        raise SystemExit(f'Emscripten {EMSCRIPTEN} is required')
    ffmpeg, integration = (source(name, arguments.bootstrap) for name in SOURCES)
    patch_xbox_adpcm(ffmpeg)
    DESTINATION.mkdir(parents=True, exist_ok=True)
    build = DESTINATION / 'build'
    build.mkdir(exist_ok=True)
    environment = os.environ.copy()
    for name in ['CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'PKG_CONFIG_PATH']:
        environment.pop(name, None)
    environment['PATH'] = str(em) + os.pathsep + environment.get('PATH', '')
    environment['SOURCE_DATE_EPOCH'] = '0'
    environment['revision'] = 'n5.1.4'
    configuration = [ffmpeg / 'configure', '--target-os=none', '--arch=x86_32', '--enable-cross-compile',
        '--disable-asm', '--disable-stripping', '--disable-programs', '--disable-doc', '--disable-debug',
        '--disable-runtime-cpudetect', '--disable-autodetect', '--disable-pthreads', '--disable-w32threads',
        '--disable-os2threads', '--disable-network', '--disable-everything', '--enable-avcodec',
        '--enable-avformat', '--enable-avfilter', '--enable-swresample', '--enable-swscale',
        '--disable-avdevice', '--disable-postproc', '--enable-protocol=file,pipe',
        '--enable-demuxer=bink,rsd,xmv,mpegvideo,wav,image2,png_pipe,bmp_pipe',
        '--enable-muxer=mpeg1video,pcm_s16le,wav,null,image2',
        '--enable-decoder=bink,wmv2,binkaudio_rdft,binkaudio_dct,pcm_s16le,pcm_s16be,pcm_u8,adpcm_ima_rad,adpcm_ima_wav,adpcm_psx,adpcm_dtk,mpeg1video,png,bmp,rawvideo',
        '--enable-encoder=mpeg1video,pcm_s16le,wrapped_avframe,png,bmp', '--enable-parser=mpegvideo,png',
        '--enable-filter=abuffer,abuffersink,buffer,buffersink,format,aformat,aresample,anull,null,fps,scale,pad,setsar,tpad,loudnorm,afade,areverse,atrim,trim',
        '--enable-zlib', '--nm=emnm', '--ar=emar', '--ranlib=emranlib', '--cc=emcc', '--cxx=em++',
        '--objcc=emcc', '--dep-cc=emcc', '--extra-cflags=-O3 -sUSE_ZLIB=1 -Wno-incompatible-function-pointer-types',
        '--extra-ldflags=-sUSE_ZLIB=1']
    if not arguments.skip_libraries:
        print('Configuring FFmpeg (compiler feature checks take a few minutes)', flush=True)
        with (DESTINATION / 'configure.log').open('w') as log:
            run(configuration, cwd=build, environment=environment, log=log)
        # FFmpeg's version recipe expands SRC_PATH without shell quoting. A
        # relative path also supports checkouts whose parent contains '&'.
        make_configuration = build / 'ffbuild/config.mak'
        make_configuration.write_text(make_configuration.read_text().replace(
            'SRC_PATH=' + str(ffmpeg), 'SRC_PATH=' + os.path.relpath(ffmpeg, build)))
        shell_configuration = build / 'ffbuild/config.sh'
        shell_configuration.write_text(shell_configuration.read_text().replace(
            'source_path=' + str(ffmpeg), 'source_path=' + os.path.relpath(ffmpeg, build)))
        print('Building FFmpeg codec and filter libraries', flush=True)
        with (DESTINATION / 'make.log').open('w') as log:
            run(['make', f'-j{arguments.jobs}'], cwd=build, environment=environment, log=log)
    frontend = integration / 'src/fftools'
    arguments = ['-O3', '-DNDEBUG', '-sUSE_ZLIB=1', '-sWASM_BIGINT=1', '-sMODULARIZE=1', '-sEXPORT_ES6=1',
        '-ffile-prefix-map=' + str(ROOT) + '=.',
        '-sEXPORT_NAME=createFFmpegCore', '-sALLOW_MEMORY_GROWTH=1', '-sINITIAL_MEMORY=33554432',
        '-sENVIRONMENT=web,worker,node', '-sEXIT_RUNTIME=0', '--no-entry',
        '-Wno-deprecated-declarations', '-Wno-incompatible-function-pointer-types',
        '-I' + str(build), '-I' + str(ffmpeg), '-I' + str(frontend),
        *[frontend / name for name in ['cmdutils.c', 'ffmpeg.c', 'ffmpeg_filter.c', 'ffmpeg_hw.c',
                                     'ffmpeg_mux.c', 'ffmpeg_opt.c', 'opt_common.c', 'ffprobe.c']],
        *[build / name / f'{name}.a' for name in ['libavfilter', 'libavformat', 'libavcodec',
                                               'libswresample', 'libswscale', 'libavutil']],
        '-sEXPORTED_FUNCTIONS=["_ffmpeg","_ffprobe","_abort","_malloc"]',
        '-sEXPORTED_RUNTIME_METHODS=["FS","setValue","getValue","UTF8ToString","lengthBytesUTF8","stringToUTF8","WORKERFS","NODEFS"]',
        '-lworkerfs.js', '-lnodefs.js', '--pre-js', integration / 'src/bind/ffmpeg/bind.js',
        '-o', DESTINATION / 'ffmpeg-core.mjs']
    print('Linking browser media converter', flush=True)
    with (DESTINATION / 'link.log').open('w') as log:
        run([em / 'emcc', *arguments], environment=environment, log=log)
    archive = archive_sources(ffmpeg, integration, emsdk)
    manifest = {'emscripten': EMSCRIPTEN,
        'sources': {name: {'url': url, 'revision': revision} for name, (url, revision) in SOURCES.items()},
        'configuration': [str(part).replace(str(ROOT), '.') for part in configuration],
        'files': {path.name: {'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                  for path in [DESTINATION / 'ffmpeg-core.mjs', DESTINATION / 'ffmpeg-core.wasm', archive]}}
    (DESTINATION / 'ffmpeg-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Media converter and corresponding source: {DESTINATION}')


if __name__ == '__main__':
    main()
