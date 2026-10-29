#!/usr/bin/env python3
"""Prepare the accepted v1.2 static browser builder without retail game data."""
import argparse
import ast
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import urllib.request
import zipfile

SITE = Path(__file__).resolve().parents[1]
REPO = SITE.parent
PROVENANCE = REPO / 'out/old3ds/runtime-provenance.json'
RUNTIME = PROVENANCE.parent / 'SRR2.elf'
ROOT_FILES = ('index.html', 'tools.html', 'styles.css', 'app.mjs', 'worker.mjs',
              'texture-worker.mjs', 'tools.mjs', 'opfs.mjs', 'checkpoints.mjs',
              'downloads.mjs', 'platform.mjs', 'service-worker.mjs', 'THIRD_PARTY_NOTICES.md')
RETAIL_SUFFIXES = {'.cia', '.3ds', '.3dsx', '.iso', '.rcf', '.p3d', '.rmv', '.3dm',
                   '.ntx', '.bundle', '.rsd', '.bin', '.pcm', '.wav', '.dds'}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def safe_name(name):
    if not isinstance(name, str) or not name or name in ('.', '..') or Path(name).name != name or '\\' in name:
        raise ValueError(f'Invalid tool filename: {name!r}')
    return name


def verified(path, expected, label):
    if digest(path) != expected.get('sha256') or ('bytes' in expected and path.stat().st_size != expected['bytes']):
        raise ValueError(f'{label} source/build manifest does not match {path.name}')
    return path


def recipe_module():
    location = SITE / 'scripts/prepare-v12-manifest.py'
    spec = importlib.util.spec_from_file_location('browser_v12_manifest', location)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def python_modules():
    # Read the literal dependency list without running browser-stage preparation.
    tree = ast.parse((SITE/'engine-conversion/v12-assets.py').read_text())
    assignment = next(node for node in tree.body if isinstance(node, ast.Assign) and
                      any(isinstance(target, ast.Name) and target.id == 'PYTHON_MODULES' for target in node.targets))
    visual = ast.literal_eval(assignment.value)
    return tuple(dict.fromkeys(('skin_inventory.py', 'prepare_native_textures.py',
                                'prepare_home_artwork.py', *visual)))


def ensure_pyodide_package(folder, lock, name):
    package = lock['packages'][name]
    filename = safe_name(package['file_name'])
    if not filename.endswith('.whl') or not re.fullmatch('[0-9a-f]{64}', package['sha256']):
        raise ValueError(f'Invalid pinned Python package metadata: {name}')
    target = folder/filename
    if not target.is_file():
        version = lock['info']['version']
        if version != '0.27.7':
            raise ValueError('The accepted image preparation uses Pyodide 0.27.7')
        # Only maintainers fetch a missing pinned wheel. The deployed browser
        # loads its runtime and packages from the same static website.
        url = f'https://cdn.jsdelivr.net/pyodide/v{version}/full/{filename}'
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(prefix='.pinned-wheel-', suffix='.part', dir=folder,
                                             delete=False) as output:
                temporary = Path(output.name)
                with urllib.request.urlopen(url, timeout=60) as response:
                    total = 0
                    while block := response.read(1024*1024):
                        total += len(block)
                        if total > 64*1024*1024:
                            raise ValueError('Pinned Python wheel exceeds the download size bound')
                        output.write(block)
            verified(temporary, package, f'Pinned {name} package')
            if target.exists():
                verified(target, package, f'Pinned {name} package')
            else:
                temporary.replace(target)
        finally:
            if temporary is not None:
                temporary.unlink(missing_ok=True)
    return verified(target, package, f'Pinned {name} package')


def validate_rsf(path, title_id):
    source = path.read_text()
    match = re.search(r'^TitleInfo:\s*\n(.*?)(?=^\S|\Z)', source, re.M | re.S)
    if not match:
        raise ValueError('The packaging configuration has no title identity')
    category = re.search(r'^\s*Category\s*:\s*(\w+)\s*$', match[1], re.M)
    unique = re.search(r'^\s*UniqueId\s*:\s*(0[xX][0-9a-fA-F]+|\d+)\s*(?:#.*)?$', match[1], re.M)
    if not category or category[1] != 'Application' or not unique:
        raise ValueError('The packaging title category or UniqueId is invalid')
    actual = f'{0x0004000000000000 | (int(unique[1], 0) << 8):016x}'
    if actual != title_id:
        raise ValueError('The packaging configuration differs from the accepted Title ID')
    if not re.search(r'^\s*SystemMode\s*:\s*64MB\b', source, re.M) or \
       not re.search(r'^\s*SystemModeExt\s*:\s*Legacy\b', source, re.M):
        raise ValueError('The packaging configuration lost original-3DS memory compatibility')


def validate_tool_manifest(folder, name, label):
    manifest = json.loads((folder/name).read_text())
    for filename, expected in manifest['files'].items():
        verified(folder/safe_name(filename), expected, label)
    return manifest


def copy_manifest_tools(folder, name, destination, label):
    manifest = validate_tool_manifest(folder, name, label)
    for filename in manifest['files']:
        target = destination / ('vendor/' + filename if filename.endswith(('.mjs', '.wasm')) else filename)
        copy(folder/filename, target)
    copy(folder/name, destination/name)
    return manifest


def assert_no_retail_files(folder):
    for path in folder.rglob('*'):
        if path.is_symlink():
            raise ValueError('A symlink reached the static website')
        if path.is_file() and path.suffix.lower() in RETAIL_SUFFIXES:
            raise ValueError(f'A retail asset or completed installer reached the static website: {path.name}')


def _prepare(runtime, manifest_path, destination):
    recipe = recipe_module().generate_recipe(provenance=manifest_path, runtime=runtime)
    if (recipe['displayVersion'], recipe['ciaVersion'], recipe['packedVersion'], recipe['titleID']) != \
       ('1.2', '1.2.0', 1056, '000400000f3a1100'):
        raise ValueError('The runtime recipe title or version differs from accepted v1.2')
    validate_rsf(REPO/'tools/3ds/cia/game.rsf', recipe['titleID'])
    for name in ROOT_FILES:
        copy(SITE/name, destination/name)
    for source in sorted((SITE/'engine-conversion').iterdir()):
        if source.is_file() and source.suffix in ('.mjs', '.py'):
            copy(source, destination/'engine-conversion'/source.name)
    rounding = SITE/'engine-conversion/v12-rounding.json'
    if rounding.is_file():
        # This optional file contains source-bound sparse rounding metadata,
        # never geometry, decoded pixels or a precomputed game asset.
        json.loads(rounding.read_text())
        copy(rounding, destination/'engine-conversion'/rounding.name)
        recipe['scenery']['roundingsSHA256'] = digest(rounding)

    tools = REPO/'out/browser-tools/wasm'
    tools_manifest = validate_tool_manifest(tools, 'tool-manifest.json', 'Packaging tool')
    for name in tools_manifest['files']:
        if not name.endswith(('.mjs', '.wasm')):
            raise ValueError(f'Unexpected packaging tool file: {name}')
        copy(tools/name, destination/'vendor'/name)
    corresponding = tools_manifest['corresponding_source']
    source_archive = verified(tools/safe_name(corresponding['file']), corresponding, 'Packaging tool corresponding source')
    copy(source_archive, destination/source_archive.name)
    copy(tools/'tool-manifest.json', destination/'tool-manifest.json')

    # Ship the small codec build with its exact corresponding source, rather
    # than the general-purpose npm core with mutable external dependencies.
    media = REPO/'out/browser-tools/minimal-ffmpeg'
    media_manifest = json.loads((media/'ffmpeg-manifest.json').read_text())
    for name in ('ffmpeg-core.mjs', 'ffmpeg-core.wasm', 'ffmpeg-sources.tar.gz'):
        verified(media/name, media_manifest['files'][name], 'Media converter')
    copy(media/'ffmpeg-core.mjs', destination/'vendor/ffmpeg-core.js')
    copy(media/'ffmpeg-core.wasm', destination/'vendor/ffmpeg-core.wasm')
    copy(media/'ffmpeg-sources.tar.gz', destination/'ffmpeg-sources.tar.gz')
    copy(media/'ffmpeg-manifest.json', destination/'ffmpeg-manifest.json')
    for source in sorted((SITE/'licenses').iterdir()):
        if source.is_file():
            copy(source, destination/'licenses'/source.name)
    extraction = REPO/'out/browser-tools/iso-extraction/wasm'
    copy_manifest_tools(extraction, 'freearc-manifest.json', destination, 'Disc extractor')
    audio = REPO/'out/browser-tools/v12-audio'
    audio_manifest = copy_manifest_tools(audio, 'v12-audio-manifest.json', destination, 'v1.2 DSP encoder')
    if digest(REPO/audio_manifest['productionSource']) != audio_manifest['productionSourceSHA256']:
        raise ValueError('The DSP encoder differs from the current production source')
    occlusion = REPO/'out/browser-tools/v12-occlusion'
    occlusion_manifest = copy_manifest_tools(occlusion, 'v12-occlusion-manifest.json', destination, 'v1.2 ray converter')
    if digest(REPO/'tools/3ds/character_occlusion.cpp') != occlusion_manifest['production_source_sha256'] or \
       digest(SITE/'scripts/occlusion_adapter.cpp') != occlusion_manifest['adapter_sha256']:
        raise ValueError('The ray converter differs from its production source or adapter')

    pyodide = REPO/'out/browser-tools/conversion/pyodide'
    lock = json.loads((pyodide/'pyodide-lock.json').read_text())
    if lock['info']['version'] != '0.27.7':
        raise ValueError('The v1.2 converter requires pinned Pyodide 0.27.7')
    for name in ('pyodide.mjs', 'pyodide.asm.js', 'pyodide.asm.wasm', 'python_stdlib.zip', 'pyodide-lock.json'):
        copy(pyodide/name, destination/'vendor/pyodide'/name)
    for name in ('pillow', 'numpy'):
        package = ensure_pyodide_package(pyodide, lock, name)
        copy(package, destination/'vendor/pyodide'/package.name)
        if name == 'numpy':
            with zipfile.ZipFile(package) as wheel:
                licenses = [path for path in wheel.namelist() if '.dist-info/' in path and
                            path.rsplit('/', 1)[-1].upper() in ('LICENSE', 'LICENSE.TXT')]
                if not licenses:
                    raise ValueError('The pinned NumPy wheel has no component license')
                (destination/'licenses/numpy-LICENSE.txt').write_bytes(wheel.read(licenses[0]))
    for name in python_modules():
        copy(REPO/'tools/3ds'/safe_name(name), destination/'python'/name)
    copy(REPO/'tools/3ds/native_texture_profile.json', destination/'native_texture_profile.json')
    copy(REPO/'tools/3ds/cia/game.rsf', destination/'game.rsf')
    (destination/'v12-recipe.json').write_text(json.dumps(recipe, indent=2)+'\n')
    # This ELF predates packaging and contains no retail RomFS; never substitute
    # a .3dsx or .cia, both of which may already contain user game assets.
    strip = Path('/opt/devkitpro/devkitARM/bin/arm-none-eabi-strip')
    subprocess.run([str(strip), '--strip-debug', '-o', str(destination/'runtime.elf'), str(runtime)], check=True)
    assert_no_retail_files(destination)
    files = {path.relative_to(destination).as_posix(): {'bytes': path.stat().st_size, 'sha256': digest(path)}
             for path in sorted(destination.rglob('*')) if path.is_file()}
    # Checkpoints are content-keyed to conversion code, pinned wheels/tools,
    # runtime, packaging settings and recipe. A converter update cannot reuse an
    # older stage even when the input game files are unchanged.
    bindings = {name: entry['sha256'] for name, entry in files.items()
                if Path(name).suffix in ('.mjs', '.js', '.wasm', '.py', '.whl', '.elf', '.rsf') or
                name in ('native_texture_profile.json', 'v12-recipe.json', 'engine-conversion/v12-rounding.json')}
    report = {
        'version': '1.2', 'packedVersion': 1056, 'titleID': recipe['titleID'],
        'sourceCommit': recipe['runtime']['sourceCommit'],
        'originalELFSHA256': recipe['runtime']['originalELFSHA256'],
        'runtimeSHA256': files['runtime.elf']['sha256'],
        'rsfSHA256': files['game.rsf']['sha256'],
        'textureProfileSHA256': files['native_texture_profile.json']['sha256'],
        'v12RecipeSHA256': files['v12-recipe.json']['sha256'],
        'v12RoundingsSHA256': files.get('engine-conversion/v12-rounding.json', {}).get('sha256'),
        'buildID': hashlib.sha256(json.dumps(bindings, sort_keys=True, separators=(',', ':')).encode()).hexdigest(),
        'minimumCIABytes': 1_000_000_000, 'files': files,
        'distribution': 'Runtime and conversion tools only. All game assets come from the selected local PC copy.',
    }
    (destination/'build-manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


def prepare(runtime, manifest_path, destination):
    runtime, manifest_path, destination = Path(runtime).resolve(), Path(manifest_path).resolve(), Path(destination).resolve()
    if destination.exists() and (not destination.is_dir() or any(destination.iterdir())):
        raise ValueError('Use a new or empty output directory; existing website files are never overwritten')
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Build privately and publish the directory only after every integrity gate.
    # A failed preparation never leaves a partially ready static website.
    with tempfile.TemporaryDirectory(prefix='.v12-builder-', dir=destination.parent) as temporary:
        staging = Path(temporary)
        report = _prepare(runtime, manifest_path, staging)
        if destination.exists():
            if any(destination.iterdir()):
                raise ValueError('The output directory changed during preparation; preserve its new files')
            destination.rmdir()
        staging.replace(destination)
    summary = {'directory': str(destination), 'files': len(report['files']) + 1,
               'bytes': sum(path.stat().st_size for path in destination.rglob('*') if path.is_file()),
               'runtime_sha256': report['runtimeSHA256'], 'buildID': report['buildID'], 'version': report['version']}
    print(json.dumps(summary))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, default=RUNTIME)
    parser.add_argument('--manifest', '--provenance', dest='manifest', type=Path, default=PROVENANCE)
    parser.add_argument('--out', type=Path, default=SITE/'dist')
    args = parser.parse_args()
    prepare(args.runtime, args.manifest, args.out)
