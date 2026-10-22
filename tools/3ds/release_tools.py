"""Locate release tools, with a pinned bootstrap for the local macOS build."""
from pathlib import Path
import hashlib
import os
import platform
import shutil
import subprocess
import urllib.request
import zipfile

# 0.19.0 drops the title's minor version (Project_CTR issue #169).
MAKEROM_VERSION = '0.18.4'
MAKEROM_PACKAGES = {
    ('Darwin', 'arm64'): ('macos_arm64', '813fd517ec23d54cacee2c9a718a3b5fda7678a541b37d19dab9f8b1de1297a2'),
    ('Darwin', 'x86_64'): ('macos_x86_64', '0d382da757833e4dc4fa99721f3cf8d9e1967cefb2a979f75d39bc4ee035f65b'),
    ('Linux', 'x86_64'): ('ubuntu_x86_64', 'dd596854718c195c6e3229286be485b122921715555af8ae5cf8e9a465d9f970'),
}
BANNERTOOL_REVISION = '734d33be79fd3f8c29c6296158f06ac7c5ca9dcb'


def locate(repo, makerom=None, bannertool=None):
    root = repo / 'out/release-tools'
    root.mkdir(parents=True, exist_ok=True)
    managed_makerom = root / f'makerom-v{MAKEROM_VERSION}'
    makerom = makerom or os.environ.get('MAKEROM') or shutil.which('makerom') or managed_makerom
    bannertool = bannertool or os.environ.get('BANNERTOOL') or shutil.which('bannertool') or root / 'bannertool-build/bannertool'
    makerom, bannertool = Path(makerom).resolve(), Path(bannertool).resolve()
    if not makerom.is_file():
        package = MAKEROM_PACKAGES.get((platform.system(), platform.machine()))
        if makerom != managed_makerom.resolve() or package is None:
            raise RuntimeError('Install makerom or pass --makerom /path/to/makerom; automatic download supports macOS and x86-64 Linux/WSL')
        print(f'Downloading makerom v{MAKEROM_VERSION}...', flush=True)
        archive = root / f'makerom-v{MAKEROM_VERSION}.zip'
        suffix, expected_hash = package
        url = f'https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v{MAKEROM_VERSION}/makerom-v{MAKEROM_VERSION}-{suffix}.zip'
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
