#!/usr/bin/env python3
"""Build a private persistent-ground fixture without overwriting source artwork.

Move only static meshes and ground intersections into the level's existing
permanent section. Interactive objects, locators, persistent-world indices and
all remaining chunks retain their original file and order. Material and texture
aliases isolate file-local names; image bytes and geometry streams stay exact.
Runtime admission and physical-console acceptance remain separate gates.
"""
import argparse
from collections import Counter, defaultdict
from copy import deepcopy
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import tempfile

from prepare_character_detail import Node, nodes
from prepare_landmark_visibility import candidates, fnv
from skin_inventory import pstring
from prepare_stream_probes import uid

ROOT, TEXTURE, SHADER = 0xff443350, 0x19000, 0x11000
STATIC, INTERSECT, MESH, GROUP = 0x3f00000, 0x3f00003, 0x10000, 0x10002
TEXTURE_PARAM = 0x11002
INTERSECT_CAPACITY = 1536


def exterior_files(level):
    """Canonical streamed sectors; mission supplements keep their normal files."""
    if level in (1, 4):
        rails, zones = ('1', '2', '3', '4a', '4b', '6', '7'), ('1', '2', '3', '4', '6', '7')
    elif level in (2, 5):
        rails = zones = ('1', '2', '3', '4')
    elif level in (3, 6):
        rails = zones = ('1', '2', '3', '4', '5')
    elif level == 7:
        rails = zones = ('1', '2', '3', '6', '7')
    else:
        raise ValueError('Invalid level')
    prefix = 'L' if level in (1, 4, 7) else 'l'
    return (f'{prefix}{level}_TERRA.p3d',
            *(f'l{level}r{value}.p3d' for value in rails),
            *(f'l{level}z{value}.p3d' for value in zones))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def name(node):
    return pstring(node.body, 0)[0]


def packed_text(value):
    value = value.encode()
    if len(value) > 254:
        raise ValueError('Asset alias exceeds Pure3D string capacity')
    return bytes([len(value)]) + value


def replace_text(body, offset, value):
    _, end = pstring(body, offset)
    return body[:offset] + packed_text(value) + body[end:]


def root(data):
    tree = nodes(data)
    if len(tree) != 1 or tree[0].kind != ROOT or tree[0].encode() != data:
        raise ValueError('Expected one losslessly parsed uncompressed Pure3D root')
    return tree[0]


def named(tree):
    result = defaultdict(list)
    for node in tree.children:
        if node.kind in (TEXTURE, SHADER):
            result[node.kind, name(node)].append(node)
    return result


def resolve(local, permanent, kind, text):
    # Accept duplicate definitions only when every byte agrees. Do not guess
    # which definition or neighboring streaming sector would satisfy a name.
    pool = local[kind, text] or permanent[kind, text]
    unique = {node.encode(): node for node in pool}
    if len(unique) != 1:
        raise ValueError(f'Unresolved or conflicting dependency {kind:08x}/{text}')
    return next(iter(unique.values()))


def groups(entity):
    meshes = [child for child in entity.children if child.kind == MESH]
    if len(meshes) != 1:
        raise ValueError('Static entity requires exactly one mesh')
    return [child for child in meshes[0].children if child.kind == GROUP]


def dependencies(entity, local, permanent):
    _, offset = pstring(entity.body, 0)
    if struct.unpack_from('<I', entity.body, offset + 4)[0]:
        raise ValueError('Translucent static entity')
    result = {}
    if not groups(entity):
        raise ValueError('Static entity has no primitive groups')
    for group in groups(entity):
        material, offset = pstring(group.body, 4)
        topology, _, count, index_count, bones = struct.unpack_from('<5I', group.body, offset)
        if topology not in (0, 1) or bones or not 0 < count <= 65535 or index_count <= 0:
            raise ValueError('Unsupported static primitive group')
        shader = resolve(local, permanent, SHADER, material)
        _, offset = pstring(shader.body, 0)
        _, offset = pstring(shader.body, offset + 4)
        if struct.unpack_from('<I', shader.body, offset)[0]:
            raise ValueError('Translucent static material')
        textures = {}
        for param in shader.children:
            if param.kind != TEXTURE_PARAM:
                continue
            texture, _ = pstring(param.body, 4)
            # Empty optional texture parameters are explicit null references.
            if texture:
                textures[texture] = resolve(local, permanent, TEXTURE, texture)
        result[material] = shader, textures
    return result


def geometry_signature(entity):
    """Every render payload byte except private object/material names."""
    clone = deepcopy(entity)
    clone.body = replace_text(clone.body, 0, '')
    for mesh in clone.children:
        if mesh.kind != MESH:
            continue
        mesh.body = replace_text(mesh.body, 0, '')
        for group in mesh.children:
            if group.kind == GROUP:
                group.body = replace_text(group.body, 4, '')
    return digest(clone.encode())


def linear_geometry_bytes(entity):
    total = 0
    for group in groups(entity):
        _, offset = pstring(group.body, 4)
        _, _, vertices, indices, _ = struct.unpack_from('<5I', group.body, offset)
        # Production picaPrimBuffer uses 36-byte vertices and 16-bit indices.
        # Include a conservative allocation-header allowance per group.
        total += 36 * vertices + ((2 * indices + 3) & ~3) + 64
    return total


def texture_upper_bytes(texture):
    _, offset = pstring(texture.body, 0)
    _, width, height, _, _, mips, *_ = struct.unpack_from('<9I', texture.body, offset)
    if not width or not height or width > 1024 or height > 1024 or mips > 11:
        raise ValueError('Unsupported texture allocation')
    width = max(8, 1 << (width - 1).bit_length())
    height = max(8, 1 << (height - 1).bit_length())
    # Upper bound even when native payload selects a smaller/16-bit format.
    return sum(max(8, width >> i) * max(8, height >> i) * 4 for i in range(max(1, mips)))


def opaque_dependencies(needed):
    """Keep foliage, animation and specialized shaders in their authored files."""
    for shader, textures in needed.values():
        _, offset = pstring(shader.body, 0)
        kind, _ = pstring(shader.body, offset + 4)
        if kind != 'simple' or any(re.search(
                r'tree|leaf|palm|bush|grass|ocean|cloud|shadow|glow|anim_light|fire|smoke',
                text, re.I) for text in textures):
            return False
    return True


def prepare(art, overlay, level, output, *, landmarks=16, landmark_vertices=10000,
            max_linear_bytes=2 * 1024 * 1024, opaque_world=False, additional_landmarks=(),
            resource_references=None, resource_fingerprint=None):
    if level not in range(1, 8) or not 0 <= landmarks <= 64 or not 0 <= landmark_vertices <= 100000 or max_linear_bytes <= 0:
        raise ValueError("Invalid level or resource budget")
    if resource_references is not None and not callable(resource_fingerprint):
        raise ValueError('Resource references require strict semantic verification')
    art, output = Path(art), Path(output)
    overlay = Path(overlay) if overlay else art
    if output.exists():
        raise ValueError('Output must be a new directory')
    paths = {p.name: p for p in art.iterdir()
             if re.fullmatch(f'l{level}[rz].*\\.p3d', p.name, re.I)}
    paths.update({p.name: p for p in overlay.iterdir()
                  if re.fullmatch(f'l{level}[rz].*\\.p3d', p.name, re.I)})
    terrain_names = {p.name for folder in (art, overlay) for p in folder.iterdir()
                     if p.name.lower() == f'l{level}_terra.p3d'}
    if len(terrain_names) != 1 or not paths:
        raise ValueError('Missing or ambiguous level artwork')
    terrain_name = terrain_names.pop()
    paths[terrain_name] = (overlay / terrain_name if (overlay / terrain_name).exists()
                           else art / terrain_name)
    if opaque_world:
        # Retail also contains mission supplements such as l3r5_dam.p3d and
        # l6r5_dam.p3d. They load beside the base sector and must retain their
        # authored names, contents and demand loading outside this override.
        expected = {value.lower() for value in exterior_files(level)}
        paths = {key: path for key, path in paths.items() if key.lower() in expected}
        if len(paths) != len(expected) or {key.lower() for key in paths} != expected:
            raise ValueError('Missing or ambiguous canonical exterior artwork')
    original = {key: path.read_bytes() for key, path in sorted(paths.items())}
    # Preserve the accepted Level 2 road-priority ordering. This transformation
    # is idempotent and keeps every chunk byte; clean retail preparation and
    # the earlier priority fixture must produce the same resident aliases.
    ordered = dict(original)
    if opaque_world and level == 2:
        from prepare_streaming_order import reorder
        for key in ('l2r2.p3d', 'l2z2.p3d'):
            if key in ordered:
                ordered[key] = reorder(ordered[key], original[terrain_name])[0]
    trees = {key: root(data) for key, data in ordered.items()}
    terrain = trees[terrain_name]
    permanent = named(terrain)
    new_textures, new_shaders, moved, intersections = {}, {}, [], []
    records, visibility, rejected, road_count = [], [], [], 0
    selected_keys = set()

    # Reuse the existing geometric/cost-based landmark selector. Give each
    # streaming file a turn so one industrial backdrop cannot consume the budget.
    if landmarks:
        pool = [item for key, path in paths.items() if key != terrain_name for item in candidates(path)]
        ranks = Counter()
        ranked = []
        for item in sorted(pool, key=lambda item: (-item['score'], item['file'], item['entity'])):
            ranked.append((ranks[item['file']], item))
            ranks[item['file']] += 1
        used_draws = used_vertices = 0
        for _, item in sorted(ranked, key=lambda pair: (pair[0], -pair[1]['score'], pair[1]['file'], pair[1]['entity'])):
            if used_draws + item['draws'] > landmarks or used_vertices + item['vertices'] > landmark_vertices:
                continue
            tree = trees[item['file']]
            entities = [node for node in tree.children if node.kind == STATIC and name(node) == item['entity']]
            if len(entities) != 1:
                continue
            try:
                dependencies(entities[0], named(tree), permanent)
            except ValueError:
                continue
            selected_keys.add((item['file'], item['entity']))
            used_draws += item['draws']
            used_vertices += item['vertices']

    aliases, resolved_references = {}, {}
    fingerprints = {}

    def texture_fingerprint(texture):
        key = id(texture)
        if key not in fingerprints:
            fingerprints[key] = resource_fingerprint(texture.encode())
        return fingerprints[key]

    for filename, tree in sorted(trees.items()):
        if filename == terrain_name:
            continue
        local, retained = named(tree), []
        for index, node in enumerate(tree.children):
            if node.kind == INTERSECT:
                intersections.append(node)
                continue
            road = node.kind == STATIC and bool(re.search(r'(^|_)track', name(node), re.I))
            landmark = node.kind == STATIC and (filename, name(node)) in selected_keys
            if node.kind != STATIC or not (road or landmark or opaque_world):
                retained.append(node)
                continue
            try:
                needed = dependencies(node, local, permanent)
            except ValueError as error:
                rejected.append(dict(file=filename, entity=name(node), reason=str(error)))
                retained.append(node)
                continue
            opaque = opaque_world and opaque_dependencies(needed)
            if not (road or landmark or opaque):
                retained.append(node)
                continue
            alias = f'ws{level}_{digest(filename.encode() + struct.pack("<I", index) + node.encode())[:20]}'
            clone = deepcopy(node)
            clone.body = replace_text(clone.body, 0, alias)
            mesh = next(child for child in clone.children if child.kind == MESH)
            mesh.body = replace_text(mesh.body, 0, alias + '_mesh')
            for material, (shader, textures) in needed.items():
                shader_identity = shader.encode() + b''.join(texture.encode() for _, texture in sorted(textures.items()))
                shader_alias = f'ws{level}_s_{digest(shader_identity)[:20]}'
                reference = None
                if resource_references is not None:
                    reference = resource_references.get(filename, {}).get(material)
                    if (not isinstance(reference, dict)
                            or reference.get('shader_sha256') != digest(shader.encode())
                            or set(reference.get('textures', {})) != set(textures)
                            or not re.fullmatch(f'ws{level}_s_[0-9a-f]{{20}}', reference.get('alias', ''))):
                        raise ValueError(f'Unrecognized resident material {filename}/{material}')
                    shader_alias = reference['alias']
                changed = deepcopy(shader)
                changed.body = replace_text(changed.body, 0, shader_alias)
                texture_references = {}
                for text, texture in textures.items():
                    existing = permanent[TEXTURE, text]
                    reuse = bool(existing) and all(n.encode() == texture.encode() for n in existing)
                    texture_reference = None
                    if reference is not None:
                        texture_reference = reference['textures'][text]
                        if (not isinstance(texture_reference, dict)
                                or texture_reference.get('semantic_sha256') != texture_fingerprint(texture)
                                or type(texture_reference.get('permanent')) is not bool):
                            raise ValueError(f'Unrecognized resident texture {filename}/{text}')
                        reuse = texture_reference['permanent']
                        if reuse and (texture_reference.get('alias') != text or not existing or any(
                                texture_fingerprint(n) != texture_reference['semantic_sha256'] for n in existing)):
                            raise ValueError(f'Unrecognized permanent texture {filename}/{text}')
                        if not reuse and not re.fullmatch(f'ws{level}_t_[0-9a-f]{{20}}', texture_reference.get('alias', '')):
                            raise ValueError(f'Invalid resident texture alias {filename}/{text}')
                    if reuse:
                        texture_alias = text
                    else:
                        texture_alias = (texture_reference['alias'] if texture_reference else
                                         f'ws{level}_t_{digest(texture.encode())[:20]}')
                        copied = deepcopy(texture)
                        copied.body = replace_text(copied.body, 0, texture_alias)
                        new_textures[texture_alias] = copied
                    if resource_fingerprint is not None:
                        texture_references[text] = dict(alias=texture_alias, permanent=reuse,
                            semantic_sha256=texture_fingerprint(texture))
                    for param in changed.children:
                        if param.kind == TEXTURE_PARAM and pstring(param.body, 4)[0] == text:
                            param.body = replace_text(param.body, 4, texture_alias)
                prior = new_shaders.get(shader_alias)
                if prior is not None and prior.encode() != changed.encode():
                    raise ValueError('Same shader bytes resolved to different texture contents')
                new_shaders[shader_alias] = changed
                if resource_fingerprint is not None:
                    entry = dict(alias=shader_alias, shader_sha256=digest(shader.encode()), textures=texture_references)
                    previous = resolved_references.setdefault(filename, {}).setdefault(material, entry)
                    if previous != entry:
                        raise ValueError(f'Conflicting resident material {filename}/{material}')
                for group in groups(clone):
                    if pstring(group.body, 4)[0] == material:
                        group.body = replace_text(group.body, 4, shader_alias)
            signature = geometry_signature(node)
            if signature != geometry_signature(clone):
                raise ValueError('Geometry changed during relocation')
            moved.append(clone)
            road_count += road
            aliases[alias] = dict(file=filename, entity=name(node), original_chunk_sha256=digest(node.encode()),
                                  geometry_sha256=signature, road=road, landmark=landmark,
                                  opaque=opaque)
            if opaque_world:
                sphere = next(child.body for child in mesh.children if child.kind == 0x10004)
                if len(sphere) != 16:
                    raise ValueError('Invalid resident sphere')
                bounds = struct.unpack('<4f', sphere)
                if not all(map(math.isfinite, bounds)) or bounds[3] <= 0:
                    raise ValueError('Invalid resident sphere')
                visibility.append((uid(alias), uid(filename.lower()), *bounds))
            if landmark:
                box = next(child.body for child in mesh.children if child.kind == 0x10003)
                sphere = next(child.body for child in mesh.children if child.kind == 0x10004)
                records.append((fnv(terrain_name.lower().encode() + b'\0' + alias.encode()), fnv(sphere + box)))
        tree.children = retained
    if resource_references is not None and resolved_references != resource_references:
        raise ValueError('Resident resource reference inventory changed')
    if not road_count or not intersections:
        raise ValueError('No ground geometry or intersections selected')
    if len(intersections) > INTERSECT_CAPACITY:
        raise ValueError('Permanent ground exceeds diagnostic section capacity')
    geometry_bytes = sum(map(linear_geometry_bytes, moved))
    texture_bytes = sum(map(texture_upper_bytes, new_textures.values()))
    if geometry_bytes + texture_bytes > max_linear_bytes:
        raise ValueError(f'Conservative linear estimate {geometry_bytes + texture_bytes} exceeds {max_linear_bytes}')
    terrain.children += list(new_textures.values()) + list(new_shaders.values()) + intersections + moved
    transformed = {key: tree.encode() for key, tree in trees.items()}
    # Assert exact-once collision preservation and untouched interactive chunks.
    before_collisions = Counter(n.encode() for data in original.values() for n in root(data).children if n.kind == INTERSECT)
    after_collisions = Counter(n.encode() for tree in trees.values() for n in tree.children if n.kind == INTERSECT)
    if before_collisions != after_collisions:
        raise ValueError('Ground collision multiset changed')
    for key, data in original.items():
        if key == terrain_name:
            continue
        untouched = lambda tree: [n.encode() for n in tree.children
                                  if n.kind not in (STATIC, INTERSECT, TEXTURE, SHADER)]
        if untouched(root(data)) != untouched(trees[key]):
            raise ValueError('Interactive or unknown chunks changed')
    for key, data in ordered.items():
        if key == terrain_name:
            continue
        retained = [n.encode() for n in root(data).children if n.kind not in (STATIC, INTERSECT)]
        if retained != [n.encode() for n in trees[key].children if n.kind not in (STATIC, INTERSECT)]:
            raise ValueError('Interactive or unknown chunks changed')
    profile_path = overlay.parent / 'landmark-visibility.bin'
    if not profile_path.exists():
        profile_path = art.parent / 'landmark-visibility.bin'
    old_records = list(additional_landmarks)
    if profile_path.exists():
        data = profile_path.read_bytes()
        magic, version, count, stride = struct.unpack_from('<4sIII', data)
        if magic != b'LMV1' or version != 1 or stride != 8 or len(data) != 16 + count * stride:
            raise ValueError('Malformed source landmark profile')
        old_records += list(struct.iter_unpack('<II', data[16:]))
    profile_records = sorted(set(old_records + records))
    if len(profile_records) > 448:
        raise ValueError('Landmark profile exceeds runtime capacity')
    output.mkdir(parents=True)
    (output / 'art').mkdir()
    for key, data in transformed.items():
        (output / 'art' / key).write_bytes(data)
    (output / 'landmark-visibility.bin').write_bytes(struct.pack('<4sIII', b'LMV1', 1, len(profile_records), 8) +
                                                  b''.join(struct.pack('<II', *record) for record in profile_records))
    if opaque_world:
        visibility.sort()
        if not 0 < len(visibility) <= 2048 or len({r[0] for r in visibility}) != len(visibility) or len({r[1] for r in visibility}) > 32:
            raise ValueError('Resident visibility catalog exceeds capacity or has duplicate identities')
        body = b''.join(struct.pack('<QQ4f', *record) for record in visibility)
        header = struct.pack('<4sIIIQQ', b'WRV1', 1, len(visibility), 32,
                             uid('art\\' + terrain_name), level)
        (output / 'resident-visibility.bin').write_bytes(header + hashlib.sha256(body).digest() + body)
    manifest = dict(experimental=True, level=level, terrain_section='art\\' + terrain_name,
                    road_meshes=road_count, landmarks=len(records),
                    opaque_world=opaque_world, resident_meshes=len(moved),
                    intersects=len(intersections), intersect_capacity=INTERSECT_CAPACITY,
                    geometry_linear_bytes=geometry_bytes, added_texture_upper_bytes=texture_bytes,
                    conservative_linear_bytes=geometry_bytes + texture_bytes,
                    collision_chunk_bytes=sum(len(n.encode()) for n in intersections),
                    collision_multiset_unchanged=True, interactive_chunks_unchanged=True,
                    geometry_streams_unchanged=True, image_payloads_unchanged=True,
                    aliases=aliases, rejected=rejected, resource_references=resolved_references,
                    sources={key: dict(path=str(paths[key].resolve()), sha256=digest(data)) for key, data in original.items()},
                    outputs={key: dict(bytes=len(data), sha256=digest(data)) for key, data in transformed.items()},
                    limitation='Prepared fixture only. Peak memory, scene capacity, missions, pixels and frame time require physical validation.')
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def admission_bytes(files, native_bundle_sha256):
    """Encode the shared runtime inventory from exact output sizes and hashes."""
    if not re.fullmatch('[0-9a-f]{64}', native_bundle_sha256):
        raise ValueError('A validated native bundle SHA-256 is required')
    if len(files) != 87 or any(len(name.encode('ascii')) >= 48 for name in files):
        raise ValueError('Unsupported exterior asset inventory')
    body = b''.join(struct.pack('<48sI32s', name.encode('ascii'), entry['bytes'], bytes.fromhex(entry['sha256']))
                    for name, entry in sorted(files.items()))
    return (struct.pack('<4sIII', b'WRA1', 1, 87, 84)+bytes.fromhex(native_bundle_sha256)+
            hashlib.sha256(body).digest()+body)


def prepare_all(art, overlay, output, *, max_linear_bytes=64*1024*1024, native_bundle_sha256=None,
                resource_fingerprint=None):
    """Publish one complete seven-level override set without changing originals.

    Build in a sibling temporary directory. A failed level cannot leave a
    partially published set that mixes prepared and original exterior files.
    Runtime admission and physical-console acceptance remain separate gates.
    """
    output = Path(output)
    if output.exists():
        raise ValueError('Output must be a new directory')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='world-residency-', dir=output.parent) as temporary:
        work = Path(temporary)
        combined = work/'all'
        (combined/'art').mkdir(parents=True)
        levels, profiles, names, references = {}, set(), set(), {}
        for level in range(1, 8):
            folder = work/str(level)
            report = prepare(art, overlay, level, folder, opaque_world=True,
                             max_linear_bytes=max_linear_bytes, resource_fingerprint=resource_fingerprint)
            if resource_fingerprint is not None:
                references[str(level)] = report['resource_references']
            for source in sorted((folder/'art').iterdir()):
                if source.name.lower() in names:
                    raise ValueError('Ambiguous exterior asset name: '+source.name)
                names.add(source.name.lower())
                (combined/'art'/source.name).write_bytes(source.read_bytes())
            (combined/f'resident-visibility{level}.bin').write_bytes((folder/'resident-visibility.bin').read_bytes())
            data = (folder/'landmark-visibility.bin').read_bytes()
            magic, version, count, stride = struct.unpack_from('<4sIII', data)
            if (magic, version, stride) != (b'LMV1', 1, 8) or len(data) != 16+count*stride:
                raise ValueError('Invalid prepared landmark profile')
            profiles.update(struct.iter_unpack('<II', data[16:]))
            levels[str(level)] = {key: report[key] for key in
                                 ('terrain_section', 'resident_meshes', 'intersects',
                                  'geometry_linear_bytes', 'added_texture_upper_bytes',
                                  'collision_multiset_unchanged', 'interactive_chunks_unchanged')}
            levels[str(level)]['sources'] = {name: entry['sha256'] for name, entry in report['sources'].items()}
        if len(profiles) > 448:
            raise ValueError('Combined landmark profile exceeds runtime capacity')
        (combined/'landmark-visibility.bin').write_bytes(
            struct.pack('<4sIII', b'LMV1', 1, len(profiles), 8)+
            b''.join(struct.pack('<II', *record) for record in sorted(profiles)))
        report = {'format': 1, 'experimental': True, 'resource_references': references,
                  'generated_by': 'tools/3ds/prepare_world_residency.py', 'levels': levels,
                  'original_assets_unchanged': True,
                  'files': {file.relative_to(combined).as_posix():
                            {'bytes': file.stat().st_size, 'sha256': digest(file.read_bytes())}
                            for file in sorted(combined.rglob('*')) if file.is_file()}}
        if native_bundle_sha256 is None:
            native_bundle_sha256 = json.loads(Path(__file__).with_name('native_texture_profile.json').read_text())['bundle_sha256']
        admission = admission_bytes(report['files'], native_bundle_sha256)
        (combined/'asset-admission.bin').write_bytes(admission)
        report['native_bundle_sha256'] = native_bundle_sha256
        report['files']['asset-admission.bin'] = {'bytes': len(admission), 'sha256': digest(admission)}
        (combined/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
        os.rename(combined, output)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('art', type=Path)
    parser.add_argument('--overlay', type=Path)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument('--level', type=int, choices=range(1, 8))
    selection.add_argument('--all-levels', action='store_true')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--landmark-draws', type=int, default=16)
    parser.add_argument('--landmark-vertices', type=int, default=10000)
    parser.add_argument('--max-linear-bytes', type=int,
                        help='Per-level conservative preparation bound; default 2 MiB for one level, 64 MiB for the complete set')
    parser.add_argument('--opaque-world', action='store_true')
    parser.add_argument('--native-bundle-sha256', help='Validated bundle identity for complete-set admission')
    args = parser.parse_args()
    if args.all_levels:
        result = prepare_all(args.art, args.overlay, args.output,
                             max_linear_bytes=args.max_linear_bytes if args.max_linear_bytes is not None else 64*1024*1024,
                             native_bundle_sha256=args.native_bundle_sha256)
        print(json.dumps({'levels': len(result['levels']), 'files': len(result['files']),
                          'experimental': True}, indent=2))
        raise SystemExit(0)
    result = prepare(args.art, args.overlay, args.level, args.output, landmarks=args.landmark_draws,
                     landmark_vertices=args.landmark_vertices,
                     max_linear_bytes=args.max_linear_bytes if args.max_linear_bytes is not None else 2*1024*1024,
                     opaque_world=args.opaque_world)
    print(json.dumps({key: value for key, value in result.items() if key not in ('sources', 'outputs', 'aliases', 'rejected')}, indent=2))
