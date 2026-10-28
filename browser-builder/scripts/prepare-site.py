#!/usr/bin/env python3
"""Prepare the static browser builder without copying any retail game data."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

SITE = Path(__file__).resolve().parents[1]
REPO = SITE.parent


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def prepare(runtime, manifest_path, destination):
    manifest = json.loads(manifest_path.read_text())
    if digest(runtime) != manifest['elf_sha256']:
        raise ValueError('The runtime is not the executable recorded in the accepted release manifest')
    destination.mkdir(parents=True, exist_ok=True)
    for name in ('index.html', 'tools.html', 'styles.css', 'app.mjs', 'worker.mjs', 'texture-worker.mjs', 'tools.mjs', 'opfs.mjs', 'THIRD_PARTY_NOTICES.md'):
        copy(SITE / name, destination / name)
    for source in (SITE / 'engine-conversion').glob('*'):
        if source.suffix in ('.mjs', '.py'):
            copy(source, destination / 'engine-conversion' / source.name)
    for source in (REPO / 'out/browser-tools/wasm').glob('*'):
        if source.suffix in ('.mjs', '.wasm'):
            copy(source, destination / 'vendor' / source.name)
    # Ship the small codec build with its exact corresponding source, rather
    # than the general-purpose npm core with mutable external dependencies.
    media = REPO / 'out/browser-tools/minimal-ffmpeg'
    media_manifest = json.loads((media / 'ffmpeg-manifest.json').read_text())
    for name in ('ffmpeg-core.mjs', 'ffmpeg-core.wasm', 'ffmpeg-sources.tar.gz'):
        if digest(media / name) != media_manifest['files'][name]['sha256']:
            raise ValueError(f'The media converter source/build manifest does not match {name}')
    copy(media / 'ffmpeg-core.mjs', destination / 'vendor/ffmpeg-core.js')
    copy(media / 'ffmpeg-core.wasm', destination / 'vendor/ffmpeg-core.wasm')
    copy(media / 'ffmpeg-sources.tar.gz', destination / 'ffmpeg-sources.tar.gz')
    copy(media / 'ffmpeg-manifest.json', destination / 'ffmpeg-manifest.json')
    tools = REPO / 'out/browser-tools/wasm'
    tools_manifest = json.loads((tools / 'tool-manifest.json').read_text())
    for name, expected in tools_manifest['files'].items():
        if digest(tools / name) != expected['sha256']:
            raise ValueError(f'The tool source/build manifest does not match {name}')
    source_archive = tools / tools_manifest['corresponding_source']['file']
    if digest(source_archive) != tools_manifest['corresponding_source']['sha256']:
        raise ValueError('The tool corresponding-source archive does not match its manifest')
    copy(source_archive, destination / source_archive.name)
    copy(tools / 'tool-manifest.json', destination / 'tool-manifest.json')
    for source in sorted((SITE / 'licenses').glob('*')):
        if source.is_file():
            copy(source, destination / 'licenses' / source.name)
    extraction = REPO / 'out/browser-tools/iso-extraction/wasm'
    extraction_manifest = json.loads((extraction / 'freearc-manifest.json').read_text())
    for name, expected in extraction_manifest['files'].items():
        if digest(extraction / name) != expected['sha256']:
            raise ValueError(f'The disc extractor source/build manifest does not match {name}')
        copy(extraction / name, destination / ('vendor/' + name if name.endswith(('.mjs', '.wasm')) else name))
    copy(extraction / 'freearc-manifest.json', destination / 'freearc-manifest.json')
    pyodide = REPO / 'out/browser-tools/conversion/pyodide'
    for name in ('pyodide.mjs', 'pyodide.asm.js', 'pyodide.asm.wasm', 'python_stdlib.zip', 'pyodide-lock.json',
                 'pillow-10.2.0-cp312-cp312-pyodide_2024_0_wasm32.whl'):
        copy(pyodide / name, destination / 'vendor/pyodide' / name)
    for name in ('skin_inventory.py', 'prepare_native_textures.py', 'prepare_home_artwork.py'):
        copy(REPO / 'tools/3ds' / name, destination / 'python' / name)
    copy(REPO / 'tools/3ds/native_texture_profile.json', destination / 'native_texture_profile.json')
    copy(REPO / 'tools/3ds/cia/game.rsf', destination / 'game.rsf')
    # This ELF predates packaging and contains no retail RomFS; never substitute a
    # .3dsx or a .cia, both of which may already contain user game assets.
    strip = Path('/opt/devkitpro/devkitARM/bin/arm-none-eabi-strip')
    subprocess.run([str(strip), '--strip-debug', '-o', str(destination / 'runtime.elf'), str(runtime)], check=True)
    report = {
        'version': manifest['version'], 'titleID': manifest['title_id'],
        'sourceCommit': manifest['source_commit'], 'originalELFSHA256': manifest['elf_sha256'],
        'runtimeSHA256': digest(destination / 'runtime.elf'),
        'rsfSHA256': digest(destination / 'game.rsf'),
        'textureProfileSHA256': digest(destination / 'native_texture_profile.json'),
        'minimumCIABytes': 1_000_000_000,
        'distribution': 'Runtime and conversion tools only. All game assets must come from the user-selected PC copy.',
    }
    (destination / 'build-manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    files = list(destination.rglob('*'))
    if any(path.suffix.lower() in ('.cia', '.rcf', '.p3d', '.rmv', '.3dm', '.ntx', '.bundle', '.rsd') for path in files if path.is_file()):
        raise ValueError('A retail asset or completed installer reached the static website')
    print(json.dumps({'directory': str(destination), 'files': sum(path.is_file() for path in files),
                      'bytes': sum(path.stat().st_size for path in files if path.is_file()), 'runtime_sha256': report['runtimeSHA256']}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, default=REPO / 'out/old3ds-build/code/SRR2.elf')
    parser.add_argument('--manifest', type=Path, default=REPO / 'out/releases/v1.1.1/Simpsons-Hit-and-Run-3DS-v1.1.1.manifest.json')
    parser.add_argument('--out', type=Path, default=SITE / 'dist')
    args = parser.parse_args()
    prepare(args.runtime, args.manifest, args.out)
