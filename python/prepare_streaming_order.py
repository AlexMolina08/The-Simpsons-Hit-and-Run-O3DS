#!/usr/bin/env python3
"""Prioritize road geometry using verbatim chunks and audited dependencies."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import struct
import os
import tempfile
from skin_inventory import chunks, pstring

TEXTURE, SHADER, STATIC, INTERSECT, GROUP, TEXTURE_PARAM = 0x19000, 0x11000, 0x3f00000, 0x3f00003, 0x10002, 0x11002
RELEASE_FILES = {
    'l2z2.p3d': ('51a395cf3e01510a2eaff2058bf7482b535867bdc6734764071f8f7ac01b22dd',
                 '2a8feb596f81412846e3a4ef42b485075b5fcf043aebcdb3fb93b141f23098ed'),
    'l2r2.p3d': ('baf5c0de6b3f87896fc32d6abade86fda9fb75ccc908f6e1f1a102543fc841dd',
                 '7d1a94b89b2d42243cc2e6149dfc28a3ae744659b87603e23a1ecc608f298467'),
}


def release_bytes(name, data, terrain):
    """Apply only the two byte-verified, physically tested v1.2 permutations."""
    fingerprints = RELEASE_FILES.get(name.lower())
    if not fingerprints:
        return data
    before, after = fingerprints
    digest = hashlib.sha256(data).hexdigest()
    if digest == after:
        return data
    if digest != before:
        raise ValueError(f'Unsupported v1.2 prepared streaming artwork: {name}')
    result, _ = reorder(data, terrain)
    if hashlib.sha256(result).hexdigest() != after:
        raise ValueError(f'Unexpected v1.2 streaming order: {name}')
    return result


def apply_release_order(art):
    """Replace staged files atomically; never overwrite a hard-linked source."""
    art = Path(art)
    terrain = (art / 'l2_TERRA.p3d').read_bytes()
    outputs = {name: release_bytes(name, (art / name).read_bytes(), terrain)
               for name in RELEASE_FILES}
    for name, data in outputs.items():
        with tempfile.NamedTemporaryFile(dir=art, prefix='.streaming-', delete=False) as staged:
            temporary = Path(staged.name)
            staged.write(data)
        try:
            os.replace(temporary, art / name)
        finally:
            temporary.unlink(missing_ok=True)
    return {name: hashlib.sha256(data).hexdigest() for name, data in outputs.items()}


def top_chunks(data):
    roots = list(chunks(data))
    if len(roots) != 1 or roots[0][0] != 0xff443350:
        raise ValueError('Expected one uncompressed Pure3D root')
    return roots[0], list(chunks(data, roots[0][2], roots[0][3]))


def walk(data, begin, end):
    for kind, body, child, stop in chunks(data, begin, end):
        yield kind, body, child, stop
        yield from walk(data, child, stop)


def reorder(data, terrain):
    root, top = top_chunks(data)
    _, permanent = top_chunks(terrain)
    resident = defaultdict(list)
    permanent_shaders = defaultdict(list)
    for kind, body, child, stop in permanent:
        if kind in (TEXTURE, SHADER):
            resident[kind].append(pstring(terrain, body)[0])
        if kind == SHADER:
            permanent_shaders[pstring(terrain, body)[0]].append(body)
    named = defaultdict(list)
    for index, (kind, body, child, stop) in enumerate(top):
        if kind in (TEXTURE, SHADER):
            named[kind, pstring(data, body)[0]].append(index)
    roads = [i for i, (kind, body, child, stop) in enumerate(top)
             if kind == STATIC and re.search(r'(^|_)track', pstring(data, body)[0], re.I)]
    textures, shaders, external = set(), set(), set()

    def require(kind, name):
        candidates = named[kind, name]
        if not candidates:
            if name not in resident[kind]:
                raise ValueError(f'Unproven permanent dependency {kind:08x}/{name}')
            if resident[kind].count(name) != 1:
                raise ValueError(f'Ambiguous permanent dependency {kind:08x}/{name}')
            external.add((kind, name))
            return None
        if len(candidates) != 1:
            raise ValueError(f'Ambiguous dependency {kind:08x}/{name}')
        return candidates[0]

    def require_opaque(source, body, name):
        _, offset = pstring(source, body)
        _, offset = pstring(source, offset + 4)
        if struct.unpack_from('<I', source, offset)[0]:
            raise ValueError(f'Translucent road material {name}')

    for index in roads:
        _, body, child, stop = top[index]
        name, offset = pstring(data, body)
        if struct.unpack_from('<I', data, offset + 4)[0]:
            raise ValueError(f'Translucent road entity {name}')
        for kind, body, sub, end in walk(data, child, stop):
            if kind != GROUP:
                continue
            material = pstring(data, body + 4)[0]
            shader = require(SHADER, material)
            if shader is None:
                permanent_bodies = permanent_shaders[material]
                if len(permanent_bodies) != 1:
                    raise ValueError(f'Ambiguous permanent shader {material}')
                require_opaque(terrain, permanent_bodies[0], material)
                continue
            shaders.add(shader)
            _, body, child, stop = top[shader]
            require_opaque(data, body, material)
            for kind, body, sub, end in chunks(data, child, stop):
                if kind == TEXTURE_PARAM:
                    texture = require(TEXTURE, pstring(data, body + 4)[0])
                    if texture is not None:
                        textures.add(texture)
    intersections = [i for i, entry in enumerate(top) if entry[0] == INTERSECT] if roads else []
    prefix = intersections + sorted(textures) + sorted(shaders) + roads
    if len(prefix) != len(set(prefix)):
        raise ValueError('Dependency groups overlap')
    moved = set(prefix)
    order = prefix + [i for i in range(len(top)) if i not in moved]
    blocks = [data[body - 12:stop] for kind, body, child, stop in top]
    result = data[:root[2]] + b''.join(blocks[i] for i in order)
    _, reordered = top_chunks(result)
    assert len(result) == len(data)
    assert Counter(hashlib.sha256(block).hexdigest() for block in blocks) == Counter(
        hashlib.sha256(result[body - 12:stop]).hexdigest() for kind, body, child, stop in reordered)
    return result, dict(road_meshes=len(roads), collision_chunks=len(intersections),
        local_textures=len(textures), local_shaders=len(shaders),
        permanent_dependencies=[dict(kind=f'{kind:08x}', name=name) for kind, name in sorted(external)],
        original_first_road_byte=top[roads[0]][1]-12 if roads else None,
        priority_prefix_bytes=sum(len(blocks[i]) for i in prefix),
        chunk_count=len(top), chunk_bytes_unchanged=True,
        remaining_relative_order_unchanged=True, original_road_order_unchanged=True)


def prepare(art, files, output):
    output.mkdir(parents=True, exist_ok=False)
    report = dict(files={}, limitation='Dependency and byte preservation do not prove runtime memory, visual or performance acceptance.')
    for name in files:
        if not re.fullmatch(r'l[1-7][rz][0-9].*\.p3d', name):
            raise ValueError(f'Exterior world file required: {name}')
        original = (art / name).read_bytes()
        ordered, audit = reorder(original, (art / (name[:2] + '_TERRA.p3d')).read_bytes())
        for variant, data in (('original', original), ('ordered', ordered)):
            path = output / variant / name
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(data)
            audit[variant + '_sha256'] = hashlib.sha256(data).hexdigest()
        audit['bytes'] = len(original)
        report['files'][name] = audit
    (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('art', type=Path)
    parser.add_argument('files', nargs='+')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.art, args.files, args.output), indent=2))
