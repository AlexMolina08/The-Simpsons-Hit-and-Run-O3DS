"""Locate release tools, with a pinned bootstrap for the local macOS build."""
from pathlib import Path
import hashlib
import os
import platform
import shutil
import subprocess
import urllib.request
import zipfile

MAKEROM_PACKAGES = {
    ('Darwin', 'arm64'): ('macos_arm64', 'ea222647bf362d1aa0d8b7570dc65e7968ce6b3d7b103e9cc815a99698d930e0'),
    ('Darwin', 'x86_64'): ('macos_x86_64', 'a52563e5549cd2dd84e9b8d1cb0d1fdf15ea9bd0f6241e2e178fe4e4bf3a9277'),
    ('Linux', 'x86_64'): ('ubuntu_x86_64', '287b809dec064e0ad597e3d272c49ecb7eed41693d5ee6fef9d8a8aa24c2497e'),
}
BANNERTOOL_REVISION = '734d33be79fd3f8c29c6296158f06ac7c5ca9dcb'


def locate(repo, makerom=None, bannertool=None):
    root = repo / 'out/release-tools'
    root.mkdir(parents=True, exist_ok=True)
    makerom = makerom or os.environ.get('MAKEROM') or shutil.which('makerom') or root / 'makerom'
    bannertool = bannertool or os.environ.get('BANNERTOOL') or shutil.which('bannertool') or root / 'bannertool-build/bannertool'
    makerom, bannertool = Path(makerom).resolve(), Path(bannertool).resolve()
    if not makerom.is_file():
        package = MAKEROM_PACKAGES.get((platform.system(), platform.machine()))
        if makerom != root.resolve() / 'makerom' or package is None:
            raise RuntimeError('Install makerom or pass --makerom /path/to/makerom; automatic download supports macOS and x86-64 Linux/WSL')
        print('Downloading makerom v0.19.0...', flush=True)
        archive = root / 'makerom.zip'
        suffix, expected_hash = package
        url = f'https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-{suffix}.zip'
        with urllib.request.urlopen(url, timeout=60) as response, archive.open('wb') as output:
            shutil.copyfileobj(response, output)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != expected_hash:
            raise RuntimeError('Downloaded makerom archive failed SHA-256 verification')
        with zipfile.ZipFile(archive) as package:
            member = next(name for name in package.namelist() if Path(name).name == 'makerom')
            makerom.write_bytes(package.read(member))
        makerom.chmod(0o755)
    if not bannertool.is_file():
        if bannertool != root.resolve() / 'bannertool-build/bannertool':
            raise RuntimeError(f'Bannertool does not exist: {bannertool}')
        print('Building pinned bannertool...', flush=True)
        source = root / 'bannertool-source'
        if not source.exists():
            subprocess.run(['git', 'clone', '--depth', '1', 'https://github.com/carstene1ns/3ds-bannertool.git', str(source)], check=True)
        revision = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
        if revision != BANNERTOOL_REVISION:
            subprocess.run(['git', '-C', str(source), 'fetch', '--depth', '1', 'origin', BANNERTOOL_REVISION], check=True)
            subprocess.run(['git', '-C', str(source), 'checkout', '--detach', BANNERTOOL_REVISION], check=True)
        # Clang rejects initialization of this variable-length array. Its maximum
        # length is three bytes; fixed storage preserves the upstream pad length.
        padding = source / 'source/3ds/lz11.cpp'
        original = 'u8 pad[padLength] = {0};'
        fixed = 'u8 pad[4] = {0};'
        text = padding.read_text()
        if original in text:
            padding.write_text(text.replace(original, fixed))
        elif fixed not in text:
            raise RuntimeError('Unexpected bannertool padding source; review before applying the portability fix')
        build = root / 'bannertool-build'
        subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release'], check=True)
        subprocess.run(['cmake', '--build', str(build), '-j', '4'], check=True)
    return makerom, bannertool
