#!/usr/bin/env python3
"""Package a built original-model ELF and prepared retail data into one CIA."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import sys

from release_version import parse
from prepare_home_menu import prepare

REPO = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default='0.1.3-rc.1', help='CIA and release version, for example 0.1.1-rc.1')
    parser.add_argument('--data', type=Path, default=REPO / 'sdcard/3ds/simpsons')
    parser.add_argument('--movies', type=Path, help='Optional separate converted movie directory')
    parser.add_argument('--elf', type=Path, default=REPO / 'out/old3ds-build/code/SRR2.elf')
    parser.add_argument('--makerom', required=True, type=Path)
    parser.add_argument('--bannertool', required=True, type=Path)
    parser.add_argument('--ffmpeg', help='FFmpeg executable used for the retail HOME Menu cue')
    parser.add_argument('--movie-validation', type=Path, help='Complete movie decode report from validate_movies.py')
    parser.add_argument('--out', type=Path, default=REPO / 'out/release/Simpsons-Hit-and-Run-3DS.cia')
    args = parser.parse_args()
    try:
        version, (major, minor, patch) = parse(args.version)
    except ValueError as error:
        parser.error(str(error))
    args.out = args.out.resolve()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    movies = args.movies or args.data / 'movies'
    for path in (args.elf, args.makerom, args.bannertool):
        if not path.is_file():
            parser.error(f'Missing build prerequisite: {path}')
    if len(list(movies.glob('*.3dm'))) < 17:
        parser.error('All 17 converted movies are required for a complete package')
    subprocess.run([sys.executable, str(REPO / 'tools/3ds/check_old3ds_memory.py'), str(args.elf)], check=True)
    home_menu = args.out.with_suffix('.home-menu')
    cue = prepare(args.data, home_menu, args.ffmpeg)
    with tempfile.TemporaryDirectory(prefix='srr2-cia-', dir=args.out.parent) as work:
        work = Path(work)
        romfs = work / 'romfs'
        romfs.mkdir()
        # Include assets only. Never package a developer's saves, logs or command.txt.
        def copy_asset(source, target):
            try:
                os.link(source, target)
            except OSError:
                shutil.copy2(source, target)
        for name in ('art', 'scripts', 'sound'):
            shutil.copytree(args.data / name, romfs / name, copy_function=copy_asset)
        for source in args.data.glob('*.rcf'):
            copy_asset(source, romfs / source.name)
        bundle = args.data / 'native-ui.bundle'
        if not bundle.is_file():
            parser.error('Missing prepared native-ui.bundle; run build_old3ds.sh first')
        copy_asset(bundle, romfs / bundle.name)
        if not (romfs / 'scripts.rcf').is_file():
            parser.error('Prepared data is missing scripts.rcf')
        (romfs / 'movies').mkdir()
        for source in movies.glob('*.3dm'):
            with source.open('rb') as movie:
                header = movie.read(12)
            if header != b'SRR3DSMV\x02\0\0\0':
                parser.error(f'Movie must use original-model format version 2: {source}')
            copy_asset(source, romfs / 'movies' / source.name)
        assets = []
        for path in sorted(romfs.rglob('*')):
            if path.is_file():
                with path.open('rb') as asset:
                    digest = hashlib.file_digest(asset, 'sha256').hexdigest()
                assets.append({'path': path.relative_to(romfs).as_posix(), 'bytes': path.stat().st_size, 'sha256': digest})
        branding = REPO / 'tools/3ds/cia'
        subprocess.run([str(args.bannertool.resolve()), 'makebanner', '-i', str(branding / 'banner.png'),
                        '-a', str(cue), '-o', str(work / 'banner.bnr')], check=True)
        subprocess.run([str(args.bannertool.resolve()), 'makesmdh', '-s', 'The Simpsons: Hit & Run',
                        '-l', 'The Simpsons: Hit & Run — Nintendo 3DS', '-p', 'Hit & Run 3DS contributors',
                        '-i', str(branding / 'icon.png'), '-r', 'regionfree', '-f', 'visible,recordusage',
                        '-o', str(work / 'icon.smdh')], check=True)
        shutil.copy2(work / 'banner.bnr', home_menu / 'banner.bnr')
        shutil.copy2(work / 'icon.smdh', home_menu / 'icon.smdh')
        temporary = work / 'game.cia'
        subprocess.run([str(args.makerom.resolve()), '-f', 'cia', '-o', str(temporary), '-target', 't',
                        '-rsf', str(branding / 'game.rsf'), '-DROMFS_ROOT=' + str(romfs),
                        '-elf', str(args.elf.resolve()), '-icon', str(work / 'icon.smdh'),
                        '-banner', str(work / 'banner.bnr'), '-major', str(major), '-minor', str(minor), '-micro', str(patch)], check=True)
        os.replace(temporary, args.out)
    with args.out.open('rb') as cia:
        digest = hashlib.file_digest(cia, 'sha256').hexdigest()
    source_commit = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=REPO, capture_output=True, text=True).stdout.strip() or None
    movie_audit = None
    if args.movie_validation:
        movie_audit = json.loads(args.movie_validation.read_text())
        packaged = {entry['path']: entry['sha256'] for entry in assets if entry['path'].startswith('movies/')}
        checked = {'movies/' + entry['file']: entry['sha256'] for entry in movie_audit['movies']}
        if packaged != checked:
            raise ValueError('Packaged movies differ from the validated conversion set')
    home_metadata = json.loads((home_menu / 'audio-source.json').read_text())
    home_metadata['banner_bytes'] = (home_menu / 'banner.bnr').stat().st_size
    home_metadata['banner_sha256'] = hashlib.sha256((home_menu / 'banner.bnr').read_bytes()).hexdigest()
    manifest = {'source_commit': source_commit, 'build_profile': 'cmake/old3ds-release.cmake', 'home_menu': home_metadata, 'version': version, 'title_id': '000400000f3a1100', 'model': 'Old and New Nintendo 3DS; 64 MiB mode; New 3DS CPU and L2 acceleration',
                'cia_bytes': args.out.stat().st_size, 'cia_sha256': digest, 'assets': assets, 'movies': movie_audit,
                'validation': 'Packaging only; installed-console validation required before release'}
    args.out.with_suffix('.manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    args.out.with_suffix('.sha256').write_text(digest + '  ' + args.out.name + '\n')
    print(f'Built complete CIA: {args.out} ({args.out.stat().st_size} bytes)', flush=True)


if __name__ == '__main__':
    main()
