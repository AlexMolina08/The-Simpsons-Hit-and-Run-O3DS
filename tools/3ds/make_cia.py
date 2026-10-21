#!/usr/bin/env python3
"""Prepare retail data, build and package a CIA locally. Never publishes a release."""
import argparse
from datetime import datetime
import os
import platform
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
from urllib.parse import quote

from release_tools import locate
from release_version import parse
from setup_sd import check_game, find_ffmpeg
from prepare_home_menu import archive_member, TRACK

ROOT = Path(__file__).resolve().parents[2]


def run(*command):
    subprocess.run([str(part) for part in command], cwd=ROOT, check=True)


def retire_builds():
    name = 'hit-and-run-build-' + datetime.now().strftime('%Y%m%d-%H%M%S')
    if platform.system() == 'Darwin':
        trash = Path.home() / '.Trash' / name
    else:
        base = Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share'))) / 'Trash'
        (base / 'info').mkdir(parents=True, exist_ok=True)
        trash = base / 'files' / name
        # Freedesktop Trash metadata lets desktop file managers restore it.
        (base / 'info' / (name + '.trashinfo')).write_text(
            '[Trash Info]\nPath=' + quote(str(ROOT / name)) + '\nDeletionDate=' + datetime.now().isoformat(timespec='seconds') + '\n')
    trash.mkdir(parents=True, exist_ok=False)
    for directory in ('old3ds-build', 'old3ds', 'deps', 'native-textures'):
        previous = ROOT / 'out' / directory
        if previous.exists():
            shutil.move(str(previous), trash / directory)
    print(f'Previous compilation outputs moved to {trash}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', nargs='?', type=Path, help='Installed retail PC game folder')
    parser.add_argument('--version', default='0.1.3-rc.1')
    parser.add_argument('--clean', action='store_true', help='Move previous compilation outputs to the system Trash before building')
    parser.add_argument('--makerom', type=Path)
    parser.add_argument('--bannertool', type=Path)
    parser.add_argument('--ffmpeg')
    args = parser.parse_args()
    if sys.version_info < (3, 11):
        parser.error('Python 3.11 or later is required')
    if args.game is None:
        if not sys.stdin.isatty():
            parser.error('Pass the installed retail PC game folder')
        entered = input('PC game folder (paste or drag the folder here): ').strip()
        if not Path(entered).is_dir():
            words = shlex.split(entered)
            if len(words) == 1:
                entered = words[0]
        args.game = Path(entered)
    game = args.game.expanduser().resolve()
    check_game(game, False)
    version, _ = parse(args.version)
    if not (game / 'art/frontend/scrooby/bootup.p3d').is_file() or len(list((game / 'movies').glob('*.rmv'))) < 17:
        parser.error('The retail installation must include boot artwork and all 17 movies')
    archive_member(game / 'music02.rcf', TRACK)
    for tool in ('git', 'bash', 'cmake', 'ninja', 'make', 'cc'):
        if not shutil.which(tool):
            parser.error(f'Missing {tool}; install the prerequisites in README.md')
    cmake_version = subprocess.check_output(['cmake', '--version'], text=True)
    match = re.search(r'(\d+)\.(\d+)\.(\d+)', cmake_version)
    if match is None or tuple(map(int, match.groups())) < (3, 28, 0):
        parser.error('CMake 3.28 or later is required')
    try:
        import PIL
    except ImportError:
        parser.error('Pillow is required: activate your Python environment and run python3 -m pip install Pillow')
    devkit = Path(os.environ.get('DEVKITPRO', '/opt/devkitpro'))
    for relative in ('cmake/3DS.cmake', 'devkitARM/bin/arm-none-eabi-gcc', 'tools/bin/tex3ds'):
        if not (devkit / relative).is_file():
            parser.error('Install the devkitPro 3ds-dev package group first')
    ffmpeg = find_ffmpeg(args.ffmpeg)
    run(ffmpeg, "-version")
    # Resolve packaging tools before retiring any previous compilation output.
    makerom, bannertool = locate(ROOT, args.makerom, args.bannertool)
    if args.clean:
        retire_builds()
    run(sys.executable, ROOT / 'tools/3ds/setup_sd.py', game, '--old3ds', '--ffmpeg', ffmpeg)
    audit = ROOT / 'out/release/movie-validation.json'
    run(sys.executable, ROOT / 'tools/3ds/validate_movies.py', game, '--movies',
        ROOT / 'sdcard/3ds/simpsons/movies', '--ffmpeg', ffmpeg, '--out', audit)
    run('bash', ROOT / 'tools/3ds/build_old3ds.sh')
    output = ROOT / 'out/release/Simpsons-Hit-and-Run-3DS.cia'
    run(sys.executable, ROOT / 'tools/3ds/build_cia.py', '--version', version,
        '--makerom', makerom, '--bannertool', bannertool, '--ffmpeg', ffmpeg,
        '--movie-validation', audit, '--out', output)
    print(f'\nReady to install: {output}\nChecksum: {output.with_suffix(".sha256")}', flush=True)


if __name__ == '__main__':
    main()
