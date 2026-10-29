"""Run the accepted v1.2 preparation on user-staged files inside browser Python.

The caller stages originals under source_root/art and persists each returned
output file before clearing the stage. Only the supplied occlusion callback can
replace a native helper; no external commands or game-data downloads occur.
"""
from contextlib import contextmanager, redirect_stdout
import hashlib
import io
import json
import math
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

PYTHON_MODULES = (
    'skin_inventory.py', 'prepare_character_detail.py', 'strip_character_detail.py',
    'prepare_character_occlusion.py', 'prepare_vehicle_reflections.py',
    'prepare_scene_occlusion.py', 'prepare_world_sun.py', 'pack_world_sun.py',
    'prepare_sprite_atlases.py', 'prepare_landmark_visibility.py',
    'prepare_streaming_order.py',
)
PYTHON_PACKAGES = ('pillow', 'numpy')
CAR_MODELS = (
    'IStruck', 'SUVA', 'ambul', 'apu_v', 'bart_v', 'bbman_v', 'bookb_v', 'burnsarm',
    'cArmor', 'cBlbart', 'cBone', 'cCellA', 'cCellB', 'cCellC', 'cCellD', 'cCola',
    'cCube', 'cCurator', 'cDonut', 'cDuff', 'cFire_v', 'cHears', 'cKlimo', 'cLimo',
    'cMilk', 'cNerd', 'cNonup', 'cPolice', 'cSedan', 'cVan', 'carhom_v', 'cletu_v',
    'comic_v', 'compactA', 'elect_v', 'famil_v', 'fishtruc', 'garbage', 'glastruc',
    'gramR_v', 'gramp_v', 'hallo', 'hbike_v', 'homer_v', 'honor_v', 'hype_v',
    'icecream', 'knigh_v', 'krust_v', 'lisa_v', 'marge_v', 'minivanA', 'moe_v',
    'mono_v', 'mrplo_v', 'nuctruck', 'oblit_v', 'otto_v', 'pickupA', 'pizza',
    'plowk_v', 'rocke_v', 'schoolbu', 'scorp_v', 'sedanA', 'sedanB', 'skinn_v',
    'snake_v', 'sportsA', 'sportsB', 'taxiA', 'tt', 'votetruc', 'wagonA', 'wiggu_v',
)
HELPER = Path('/__browser_v12_occlusion__')


def _digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _notify(callback, stage, detail, completed=0, total=0):
    if callback:
        callback(stage, detail, completed, total)


@contextmanager
def _helper(callback):
    """Intercept exactly the production ray helper, restoring subprocess after use."""
    original = subprocess.run

    def run(command, *positional, **options):
        if positional or not isinstance(command, (list, tuple)) or not command or str(command[0]) != str(HELPER):
            raise RuntimeError('An unsupported external process was requested during local preparation.')
        arguments = list(command[1:])
        sun = None
        if arguments:
            if len(arguments) != 4 or arguments[0] != '--sun':
                raise RuntimeError('The local ray helper received unsupported arguments.')
            sun = tuple(float(value) for value in arguments[1:])
            if not all(math.isfinite(value) for value in sun):
                raise ValueError('Invalid sunlight direction.')
        if not callable(callback):
            raise RuntimeError('The local v1.2 ray converter is unavailable.')
        request = options.get('input')
        if not isinstance(request, bytes) or len(request) < 16:
            raise ValueError('The local ray helper received an incomplete request.')
        result = callback(request, sun)
        if hasattr(result, 'to_py'):
            result = result.to_py()
        output = bytes(result)
        if len(output) != struct.unpack_from('<I', request)[0]:
            raise ValueError('The local ray helper returned incomplete output.')
        return subprocess.CompletedProcess(command, 0, output, b'')

    subprocess.run = run
    try:
        yield
    finally:
        subprocess.run = original


def _main(module, arguments):
    previous = sys.argv
    sys.argv = [module.__name__, *(str(value) for value in arguments)]
    try:
        # The page reports deliberate progress rather than multi-page CLI JSON.
        with redirect_stdout(io.StringIO()):
            module.main()
    finally:
        sys.argv = previous


def _art(root):
    path = root/'art'
    if not path.is_dir():
        raise ValueError('The selected game has no staged artwork folder.')
    return path


def _copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)


def _outputs(directory):
    return [{'path': str(path.relative_to(directory)), 'bytes': path.stat().st_size,
             'sha256': _digest(path)} for path in sorted(directory.rglob('*')) if path.is_file()]


def _choose_landmarks(art, level):
    import prepare_landmark_visibility as landmarks
    paths = [path for path in sorted(art.glob('*.p3d'))
             if re.fullmatch('l'+level+r'[rz].*\.p3d', path.name, re.I)]
    if not paths:
        raise ValueError(f'Map {level} artwork is missing from local preparation.')
    pool = [item for path in paths for item in landmarks.candidates(path)]
    # Identical balanced-files selection to the native v1.2 preparation.
    ranked = sorted(pool, key=lambda item: (-item['score'], item['file'], item['entity']))
    ranks, ordered = {}, []
    for item in ranked:
        rank = ranks.get(item['file'], 0)
        ranks[item['file']] = rank+1
        ordered.append((rank, item))
    ranked = [item for _, item in sorted(ordered, key=lambda pair:
              (pair[0], -pair[1]['score'], pair[1]['file'], pair[1]['entity']))]
    draws = vertices = 0
    selected = []
    for item in ranked:
        if draws+item['draws'] > 16 or vertices+item['vertices'] > 10000:
            continue
        draws += item['draws']
        vertices += item['vertices']
        selected.append(item)
    return {'level': level, 'selected': selected, 'draws': draws, 'vertices': vertices}


def _write_landmarks(levels, destination):
    if isinstance(levels, str):
        levels = json.loads(levels)
    if sorted(str(level['level']) for level in levels) != list('1234567'):
        raise ValueError('All seven maps must be prepared before finalizing distant scenery.')
    records = sorted((item['name_key'], item['geometry_key'])
                     for level in levels for item in level['selected'])
    if len(records) != len(set(records)) or len(records) > 448:
        raise ValueError('Invalid distant-scenery index.')
    binary = struct.pack('<4sIII', b'LMV1', 1, len(records), 8)+b''.join(
        struct.pack('<II', *record) for record in records)
    (destination/'landmark-visibility.bin').write_bytes(binary)


def _round_scene(data, source_hash, record):
    """Restore measured ARM baker math behavior, never substituting source pixels.

    A correction is valid only for one source and one exact locally baked file.
    Deltas may touch RGB lanes of existing colour-list records only; geometry,
    alpha and PNG bytes remain outside this narrow compatibility adjustment.
    """
    if not record:
        return data, 0
    if source_hash != record['source_sha256']:
        raise ValueError('This game copy has an unsupported scenery-lighting fingerprint.')
    digest = hashlib.sha256(data).hexdigest()
    if digest == record['accepted_sha256']:
        return data, 0
    if digest != record['strict_sha256']:
        raise ValueError('This browser produced an unrecognized scenery-lighting result.')
    from prepare_scene_occlusion import walk
    colour_ranges = []
    for kind, body, child, stop in walk(data):
        if kind == 0x10008:
            count = struct.unpack_from('<I', data, body)[0]
            if child-body != 4+count*4:
                raise ValueError('An existing scenery colour list has an invalid size.')
            colour_ranges.append((body+4, child))
    rgb_bytes = sum((end-begin)//4*3 for begin, end in colour_ranges)
    if len(record['deltas']) > rgb_bytes*.001:
        raise ValueError('A nonsparse scenery math-restoration record was rejected.')
    result = bytearray(data)
    seen = set()
    for offset, delta in record['deltas']:
        if offset in seen or not isinstance(delta, int) or isinstance(delta, bool) or not 1 <= abs(delta) <= 4 or not any(
                begin <= offset < end and (offset-begin) % 4 < 3
                for begin, end in colour_ranges):
            raise ValueError('An invalid scenery-rounding adjustment was rejected.')
        seen.add(offset)
        value = result[offset]+delta
        if not 0 <= value <= 255:
            raise ValueError('An invalid scenery-rounding value was rejected.')
        result[offset] = value
    if hashlib.sha256(result).hexdigest() != record['accepted_sha256']:
        raise ValueError('Scenery lighting did not match the accepted v1.2 output.')
    return bytes(result), len(seen)


def prepare_stage(stage, source_root, output_root, level=None, occlusion=None,
                  progress=None, landmark_levels=None, rounding=None):
    """Prepare one bounded stage and return output paths/hashes plus small metadata.

    Stages: cars, characters, hud, world (one level), landmarks. The world result
    includes a `landmarks` record; pass all seven records to the landmarks stage
    as landmark_levels (a JSON string is accepted). The caller can then release
    each level's MEMFS files before staging the next level.

    occlusion(request_bytes, sun_direction_or_none) returns one shade byte per
    requested vertex. progress(stage, detail, completed, total) is optional.
    Originals are never opened for writing. output_root must be a fresh folder.
    """
    source = Path(source_root)
    output = Path(output_root)
    output.mkdir(parents=True, exist_ok=False)
    metadata = {}
    with tempfile.TemporaryDirectory(prefix='srr-v12-assets-') as temporary, _helper(occlusion):
        work = Path(temporary)
        if stage == 'cars':
            import prepare_vehicle_reflections as cars
            _notify(progress, stage, 'Preparing vehicle reflections')
            prepared = work/'cars'
            _main(cars, [_art(source)/'cars', '--models', *CAR_MODELS,
                         '--fallback-map', 'EnvMap.bmp', '--output', prepared])
            records = json.loads((prepared/'manifest.json').read_text())
            if len(records) != len(CAR_MODELS):
                raise ValueError('This game copy does not contain all supported vehicle artwork.')
            for record in records:
                _copy(prepared/record['file'], output/'art/cars'/record['file'])
            metadata['models'] = len(records)
        elif stage == 'characters':
            import prepare_character_detail as detail
            import prepare_character_occlusion as ao
            import strip_character_detail as strips
            originals = _art(source)/'chars'
            refined = work/'refined'
            _notify(progress, stage, 'Preparing character detail')
            _main(detail, [originals, '--budget', '64', '--output', refined])
            changed = json.loads((refined/'manifest.json').read_text())
            if len(changed) != 43:
                raise ValueError('This game copy does not contain all supported character artwork.')

            class CharacterInputs:
                def glob(self, pattern):
                    # AO was accepted on refined meshes before strip conversion.
                    # Supply that overlay without copying every original again.
                    return [refined/path.name if (refined/path.name).is_file() else path
                            for path in originals.glob(pattern)]

            calls = 0

            def character_occlusion(request, sun):
                nonlocal calls
                calls += 1
                _notify(progress, stage, 'Preparing character shading', calls, 110)
                return occlusion(request, sun)

            _notify(progress, stage, 'Preparing character shading')
            # One nested helper scope supplies progress without changing the
            # geometry parser, ray calculations, signatures or output records.
            with _helper(character_occlusion):
                records, sources, _ = ao.prepare(CharacterInputs(), HELPER, 64)
            offset = 16+20*len(records)
            index, payload = bytearray(), bytearray()
            for key, shades in sorted(records.items()):
                index.extend(struct.pack('<5I', *key, offset+len(payload)))
                payload.extend(shades)
            (output/'character-occlusion.bin').write_bytes(
                b'SRRAO001'+struct.pack('<II', len(records), offset+len(payload))+index+payload)
            for record in changed:
                encoded, _ = strips.prepare((refined/record['file']).read_bytes())
                destination = output/'art/chars'/record['file']
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(encoded)
            metadata.update(models=len(changed), ao_records=len(records), ao_sources=len(sources))
        elif stage == 'hud':
            import prepare_sprite_atlases as sprites
            _notify(progress, stage, 'Preparing the HUD atlas')
            target = output/'art/frontend/scrooby/ingame.p3d'
            report = sprites.prepare(_art(source)/'frontend/scrooby/ingame.p3d', target, 160*1024)
            # The preparation report is maintenance metadata, not a game asset.
            target.with_suffix('.atlas.json').unlink()
            metadata['atlases'] = report['sprites']
        elif stage == 'world':
            import prepare_scene_occlusion as scene
            import prepare_world_sun as sun
            import pack_world_sun as pack
            if str(level) not in tuple('1234567'):
                raise ValueError('Select one valid map to prepare.')
            level = str(level)
            art = _art(source)
            metadata['landmarks'] = _choose_landmarks(art, level)
            baked, sunlight, packed = work/'baked', work/'sunlight', work/'packed'
            _notify(progress, stage, f'Preparing map {level} surface lighting')
            _main(scene, [art, '--helper', HELPER, '--output', baked, '--levels', level,
                          '--radius', '2.25', '--key-fill', '--sun-shadows'])
            rounding = json.loads(rounding) if isinstance(rounding, str) else rounding or {}
            corrections = rounding.get('files', {})
            corrected = 0
            originals = {path.name.lower(): path for path in art.glob('*.p3d')}
            for path in baked.glob('*.p3d'):
                original = originals[path.name.lower()]
                data, count = _round_scene(path.read_bytes(), _digest(original), corrections.get(path.name.lower()))
                if count:
                    path.write_bytes(data)
                corrected += count
            metadata['scene_rounding_bytes'] = corrected
            # Preserve exact vertex lighting even in files with no projected
            # ground group; the world-sun packer writes only eligible files.
            for path in baked.glob('*.p3d'):
                _copy(path, output/'art'/path.name)
            for path in art.glob('*.p3d'):
                if re.fullmatch('l'+level+r'[rz].*\.p3d', path.name, re.I) and not (baked/path.name.lower()).exists():
                    _copy(path, baked/path.name.lower())
            night = level in '467'
            _notify(progress, stage, f'Preparing map {level} sun and contact shadows')
            _main(sun, [art, '--level', level, '--output', sunlight, '--helper', HELPER,
                        '--size', '1024', '--sun-strength', '.04' if night else '.30',
                        '--contact-strength', '.33' if night else '.50'])
            _notify(progress, stage, f'Packing map {level} lighting')
            _main(pack, [art, '--baked', baked, '--map', sunlight, '--output', packed,
                         '--preserve-sort', '--size', '512'])
            for path in packed.glob('*.p3d'):
                _copy(path, output/'art'/path.name)
            if level == '2':
                from prepare_streaming_order import apply_release_order
                metadata['streaming_road_order'] = apply_release_order(output/'art')
            metadata['level'] = level
        elif stage == 'landmarks':
            _notify(progress, stage, 'Preparing distant scenery')
            if landmark_levels is None:
                landmark_levels = [_choose_landmarks(_art(source), value) for value in '1234567']
            _write_landmarks(landmark_levels, output)
        else:
            raise ValueError(f'Unknown v1.2 preparation stage: {stage}')
    outputs = _outputs(output)
    _notify(progress, stage, 'Preparation complete', len(outputs), len(outputs))
    return {'stage': stage, 'files': outputs, **metadata}
