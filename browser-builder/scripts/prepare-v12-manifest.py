#!/usr/bin/env python3
"""Export accepted v1.2 build settings and hashes, never game asset payloads."""
import argparse
import hashlib
import json
import mmap
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
LOCAL = ROOT / 'out/local-dev/visual-upgrade-20261025'
DEFAULT_PROVENANCE = ROOT / 'out/old3ds/runtime-provenance.json'
DEFAULT_OVERLAY = LOCAL / 'v12-accepted-asset-overlay'
DEFAULT_REFERENCE = ROOT / 'out/browser-tools/iso-extraction/pc-game'
DEFAULT_OUTPUT = ROOT / 'out/browser-v12/v12-recipe.json'
sys.path.insert(0, str(ROOT/'tools/3ds'))
sys.path.insert(0, str(ROOT/'browser-builder/engine-conversion'))
from importlib import import_module
semantic_fingerprint = import_module('asset-semantics').semantic_fingerprint
from prepare_streaming_order import RELEASE_FILES, release_bytes


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def read_json(path):
    return json.loads(path.read_text())


def dialogue_keys():
    result = {}
    for code, name in (('en', 'english'), ('fr', 'french'), ('es', 'spanish')):
        script = (ROOT/f'code/sound/soundrenderer/scripts/{name}.cpp').read_text()
        paths = re.findall(r'\{\s*"[^"]+",\s*"([^"]+\.rsd)"\s*\}', script)
        require(bool(paths), f'The {name} dialogue script has no resource index')
        keys = set()
        for path in paths:
            value = 0
            for character in path.replace('/', '\\'):
                byte = ord(character)
                if byte < 97:
                    byte += 32
                value = (31 * value + byte) & 0xffffffff
            keys.add(value)
        result[code] = sorted(keys)
    return result


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate_runtime(provenance=DEFAULT_PROVENANCE, native_index=None, runtime=None):
    """Validate the unstripped source ELF before the website makes its own copy."""
    provenance = Path(provenance).resolve()
    metadata = read_json(provenance)
    for field in ('native_bench', 'runtime_profile', 'receiver_hook'):
        require(metadata.get(field) is False, f'Accepted runtime must disable {field}')
    require(metadata.get('visual_features_default') == 255, 'Accepted runtime must retain visual mask 255')
    require(metadata.get('early_opaque_queue') is True, 'Accepted stereo submission improvement is missing')
    require(metadata.get('version') == '1.2', 'The browser builder requires the accepted v1.2 runtime')
    require(metadata.get('streaming_policy') == 1, 'Rebuild v1.2 with the committed streaming fixes')
    require(metadata.get('language_config_version') == 1 and
            metadata.get('languages') == ['en', 'fr', 'es'],
            'Build the multilingual v1.2 runtime before browser packaging')
    require(metadata.get('source_tree_dirty') is False, 'Commit and rebuild the v1.2 runtime before browser packaging')
    require(re.fullmatch(r'[0-9a-f]{40}', metadata.get('source_commit', '')),
            'The accepted runtime has no complete source commit')
    runtime = Path(runtime or metadata['elf']).resolve()
    require(digest(runtime) == metadata.get('elf_sha256'), 'Accepted runtime ELF hash mismatch')
    with runtime.open('rb') as source:
        header = source.read(20)
        require(header[:5] == b'\x7fELF\x01' and len(header) == 20 and
                int.from_bytes(header[18:20], 'little') == 40,
                'The runtime must be the standalone 32-bit ARM ELF, without game RomFS')
        with mmap.mmap(source.fileno(), 0, access=mmap.ACCESS_READ) as data:
            for marker in (b'OLD3DS_PROFILE', b'OLD3DS_DUMP ', b'OLD3DS_DEV_RETURN',
                           b'bench_route=', b'NATIVE_EARLY_QUEUE', b'NATIVE_BUILD ', b'GPU_SKIN_SUMMARY'):
                require(data.find(marker) < 0, f'Instrumented runtime marker: {marker.decode()}')
    cache_path = provenance.parent / 'CMakeCache.txt'
    require(digest(cache_path) == metadata.get('cmake_cache_sha256'), 'Accepted runtime CMake cache hash mismatch')
    cache = cache_path.read_text()
    require(re.search(r'^SRR2_REGION:STRING=PAL$', cache, re.M),
            'The multilingual runtime requires the PAL dialogue scripts')
    off = ['SRR2_MICROPROFILE'] + ['SRR2_OLD3DS_' + field for field in (
        'NATIVE_BENCH', 'OPTIONAL_DIAG', 'CAUSAL_DIAG', 'MEMORY_DIAG', 'COARSE_POOLS',
        'RECURRING_PROFILE', 'MAIN_PROFILE', 'SUBMIT_PROFILE', 'VEHICLE_PROFILE',
        'GPU_SKIN_AB', 'GPU_SKIN_METRICS', 'COLLVOL_VERIFY')]
    for flag in off:
        require(re.search(r'^' + flag + r':BOOL=OFF$', cache, re.M), f'Release flag must be OFF: {flag}')
    on = ['SRR2_OLD3DS_' + field for field in ('VISUAL_UPGRADE', 'DISTANCE_HAZE', 'CHARACTER_FORM',
          'NATIVE_TEXTURES', 'NATIVE_ETC1', 'NATIVE_TEXTURE_BUNDLE', 'PREPARED_AUDIO', 'SPRITE_ATLAS')]
    for flag in on:
        require(re.search(r'^' + flag + r':BOOL=ON$', cache, re.M), f'Accepted feature must be ON: {flag}')
    require(re.search(r'^SRR2_OLD3DS_EXPERIMENT_ID:STRING=$', cache, re.M), 'The accepted runtime has an experiment ID')
    budget = metadata.get('fixed_startup_budget', {})
    require(budget.get('application_budget') == 64 * 1024 * 1024 and budget.get('pass_budget') is True,
            'Accepted runtime exceeds or omits the original-3DS startup budget')
    native_index = Path(native_index or ROOT/'out/native-textures/bundle_index.hpp').resolve()
    require(digest(native_index) == metadata.get('accepted_native_index_sha256'), 'Accepted native texture index hash mismatch')
    return metadata, runtime, native_index


def generate_recipe(provenance=DEFAULT_PROVENANCE, overlay=DEFAULT_OVERLAY,
                    reference=DEFAULT_REFERENCE, native_index=None, runtime=None):
    metadata, _, native_index = validate_runtime(provenance, native_index, runtime)
    overlay, reference = Path(overlay).resolve(), Path(reference).resolve()
    base = ROOT/'sdcard/3ds/simpsons'
    profile_path = ROOT/'tools/3ds/native_texture_profile.json'
    profile = read_json(profile_path)
    header = native_index.read_text()
    bundle_hash = re.search(r'BundleSHA256\[\]="([0-9a-f]{64})"', header)
    bundle_bytes = re.search(r'BundleBytes=(\d+)u', header)
    bundle_entries = re.search(r'EntryCount=(\d+)u', header)
    require(bundle_hash and bundle_bytes and bundle_entries, 'The accepted native index metadata is incomplete')
    require(profile.get('bundle_sha256') == bundle_hash[1], 'Browser texture profile does not match the accepted native index')
    require(len(profile.get('source_variant_keys', [])) == int(bundle_entries[1]), 'Browser texture profile entry count mismatch')

    cars = read_json(LOCAL/'vehicle-glass-all-eligible/manifest.json')
    characters = read_json(LOCAL/'refined-characters-strips/manifest.json')
    ao = read_json(LOCAL/'prepared-bounded/character-occlusion.json')
    atlas = read_json(LOCAL/'sprite-atlas-pilot/ingame.atlas.json')
    landmark = read_json(LOCAL/'landmark-profile-balanced/manifest.json')
    require(len(cars) == 75 and len(characters) == 43, 'Accepted car/character selection count changed')
    expected, changes, known_sources = {}, {}, {}
    for path in sorted(overlay.rglob('*')):
        if not path.is_file():
            continue
        name = path.relative_to(overlay).as_posix()
        item = {'sha256': digest(path), 'bytes': path.stat().st_size,
                'required': path.suffix != '.json'}
        data = path.read_bytes()
        if name in ('art/l2z2.p3d', 'art/l2r2.p3d'):
            data = release_bytes(path.name, data, (overlay/'art/l2_TERRA.p3d').read_bytes())
            item['sha256'] = hashlib.sha256(data).hexdigest()
        if path.suffix.lower() == '.p3d':
            item['semantic_sha256'] = semantic_fingerprint(data)
        base_path = base/name
        item['change'] = ('new' if not base_path.is_file() else
                          'unchanged' if digest(base_path) == item['sha256'] else 'changed')
        source = reference/name
        if source.is_file():
            item['sourceSHA256'] = digest(source)
            known_sources[name] = {'sha256': item['sourceSHA256'], 'bytes': source.stat().st_size}
        expected[name] = item
        if item['change'] != 'unchanged':
            changes[name] = item
    counts = {kind: sum(item['change'] == kind for item in expected.values())
              for kind in ('changed', 'unchanged', 'new')}
    require(counts == {'changed': 198, 'unchanged': 74, 'new': 3}, 'Accepted overlay file counts changed')
    for group, directory in ((cars, 'cars'), (characters, 'chars')):
        for record in group:
            name = f'art/{directory}/{record["file"]}'
            require(expected.get(name, {}).get('sha256') == record['output_sha256'], f'Accepted model output mismatch: {name}')
            require(known_sources.get(name, {}).get('sha256') == record['source_sha256'], f'Known original PC source mismatch: {name}')
    require(expected['art/frontend/scrooby/ingame.p3d']['sha256'] == atlas['output_sha256'], 'Accepted HUD atlas hash mismatch')
    require(expected['character-occlusion.bin']['sha256'] == ao['sha256'], 'Accepted character AO hash mismatch')

    # All scene and AO context inputs participate in the known-copy fingerprint.
    # A different occluder can change a bake even when its target file matches.
    context_paths = set(reference.joinpath('art').glob('l[1-7]*.p3d'))
    context_paths.update(reference/'art/chars'/name for name in ao['sources'])
    context_paths.update(reference/name for name in ('soundfx.rcf', 'carsound.rcf', 'dialog.rcf',
        'art/frontend/scrooby/frontend.p3d', 'art/frontend/scrooby/resource/pure3d/logo_anim.p3d'))
    for path in sorted(context_paths):
        require(path.is_file(), f'Known original PC source missing: {path.name}')
        name = path.relative_to(reference).as_posix()
        if name not in known_sources:
            known_sources[name] = {'sha256': digest(path), 'bytes': path.stat().st_size}
    source_manifest = json.dumps(known_sources, sort_keys=True, separators=(',', ':')).encode()

    cache = (overlay/'sound/3ds-dsp-clips.bin').read_bytes()
    require(len(cache) >= 32, 'Accepted audio cache is truncated')
    magic, version, clips, size, checksum, predictors, reserved = struct.unpack_from('<8s6I', cache)
    require((magic, version, clips, size, predictors, reserved) ==
            (b'SRRDSP1\0', 1, 38, len(cache), 3, 0), 'Accepted audio cache header changed')
    samples = [struct.unpack_from('<I', cache, 32 + i*16)[0] for i in range(clips)]
    worlds = []
    for level in range(1, 8):
        contact = read_json(LOCAL/f'world-sun-prototype/l{level}-contact-variant/manifest.json')
        packed = read_json(LOCAL/f'world-sun-prototype/packed-sort-l{level}-512/manifest.json')
        require(packed.get('sort_keys_preserved') is True, 'Accepted world bake must preserve material sorting')
        worlds.append({'level': level, 'sourceSize': contact['size'], 'packedSize': 512,
                       'sunStrength': contact['sun_strength'], 'contactStrength': contact['contact_strength'],
                       'preserveSort': True, 'topSurfaceOnly': contact['top_surface_only'],
                       'alphaFoliageOccluders': contact['alpha_foliage_occluders']})
    return {
        'schemaVersion': 1, 'semanticFingerprintVersion': 1,
        'displayVersion': '1.2', 'ciaVersion': '1.2.0', 'packedVersion': 1056,
        'titleID': '000400000f3a1100', 'visualFeatures': 255,
        'runtime': {'sourceCommit': metadata['source_commit'], 'originalELFSHA256': metadata['elf_sha256'],
                    'cmakeCacheSHA256': metadata['cmake_cache_sha256'], 'diagnosticsEnabled': False,
                    'receiverHook': False, 'earlyOpaqueQueue': True},
        'nativeTextures': {'path': 'native146.bundle', 'sha256': bundle_hash[1], 'bytes': int(bundle_bytes[1]),
                           'entries': int(bundle_entries[1]), 'indexSHA256': metadata['accepted_native_index_sha256'],
                           'profileSHA256': digest(profile_path)},
        'vehicleModels': [record['file'][:-4] for record in cars],
        'characterFiles': [record['file'] for record in characters],
        'characterModels': [record['file'].removesuffix('_m.p3d') for record in characters],
        'characterDetail': {'budget': 64, 'stripAfterOcclusion': True},
        'characterOcclusion': {'format': ao['format'], 'samples': ao['samples'], 'records': ao['records'],
            'sourceStage': 'detail-before-strip', 'sourceHashesAtStage': ao['sources'],
            'bytes': ao['bytes'], 'sha256': ao['sha256']},
        'hudAtlas': {'extraTextureKiB': atlas['extra_budget_bytes'] // 1024,
                     'sprites': atlas['sprites'], 'outputSHA256': atlas['output_sha256']},
        'scenery': {'radius': 2.25, 'keyFill': True, 'sunShadows': True, 'sunSamples': 8, 'levels': worlds},
        'landmarks': {'drawBudget': landmark['draw_budget'], 'vertexBudget': landmark['vertex_budget'],
                       'allocation': landmark['allocation']},
        'audioCache': {'path': 'sound/3ds-dsp-clips.bin', 'format': 'SRRDSP1', 'version': 1,
                       'clips': clips, 'predictors': predictors, 'bytes': size, 'checksum': checksum,
                       'sha256': expected['sound/3ds-dsp-clips.bin']['sha256'],
                       'sampleLengths': sorted(set(samples)), 'archives': ['soundfx.rcf', 'carsound.rcf'],
                       'pcmKey': 'exact complete PCM bytes', 'deduplicatePCM': True},
        'knownCopy': {'description': 'Validated English PC asset layout', 'sourceFiles': known_sources,
                       'sourceManifestSHA256': hashlib.sha256(source_manifest).hexdigest(),
                       'exactOutputChecksRequireMatchingInputs': True},
        'streamingPolicy': {'version': 1, 'roadFiles': list(RELEASE_FILES), 'chunkBodiesUnchanged': True},
        'hudVariants': read_json(ROOT/'browser-builder/engine-conversion/localized-assets.json')['hudVariants'],
        'dialogueKeys': dialogue_keys(),
        'overlayCounts': counts, 'requiredOverrides': changes, 'expectedFiles': expected,
    }


def prepare_manifest(output=DEFAULT_OUTPUT, **arguments):
    recipe = generate_recipe(**arguments)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(recipe, indent=2) + '\n')
    return recipe


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--provenance', type=Path, default=DEFAULT_PROVENANCE)
    parser.add_argument('--overlay', type=Path, default=DEFAULT_OVERLAY)
    parser.add_argument('--reference', type=Path, default=DEFAULT_REFERENCE)
    parser.add_argument('--native-index', type=Path)
    parser.add_argument('--runtime', type=Path)
    parser.add_argument('--out', type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    recipe = prepare_manifest(args.out, provenance=args.provenance, overlay=args.overlay,
                              reference=args.reference, native_index=args.native_index, runtime=args.runtime)
    print(json.dumps({'output': str(args.out), 'metadataOnly': True,
                      'overrides': len(recipe['requiredOverrides']), 'expectedFiles': len(recipe['expectedFiles']),
                      'cars': len(recipe['vehicleModels']), 'characters': len(recipe['characterFiles']),
                      'sourceCommit': recipe['runtime']['sourceCommit']}, indent=2))
