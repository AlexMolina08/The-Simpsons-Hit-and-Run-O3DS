#!/usr/bin/env python3
"""Prepare one bounded, texture-free architecture backdrop in a private TERRA copy.

This is an experimental visual asset. It needs the explicit far-backdrop loader
and sort policy; do not install it as a normal opaque or shadow entity.
"""
import argparse
import copy
import hashlib
import io
import json
import re
import struct
from pathlib import Path

import numpy as np
from PIL import Image

from prepare_character_detail import Node, nodes, triangles
from prepare_landmark_visibility import candidates
from prepare_vehicle_reflections import pstring, string, walk


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def texture_means(tree):
    result = {}
    for texture in walk(tree):
        if texture.kind != 0x19000:
            continue
        name, _ = pstring(texture.body, 0)
        for image in walk(texture.children):
            if image.kind != 0x19002:
                continue
            size, = struct.unpack_from('<I', image.body)
            payload = image.body[4:4+size]
            if not payload.startswith((b'\x89PNG', b'DDS ')):
                continue
            with Image.open(io.BytesIO(payload)) as decoded:
                pixels = np.asarray(decoded.convert('RGB'), dtype=np.float64)
                result[name] = pixels.mean(axis=(0, 1))
            break
    return result


def material_data(tree):
    result = {}
    for material in walk(tree):
        if material.kind != 0x11000:
            continue
        name, offset = pstring(material.body, 0)
        shader, _ = pstring(material.body, offset+4)
        allowed, texture = shader == 'simple', ''
        for parameter in material.children:
            tag = parameter.body[:4]
            if parameter.kind == 0x11003 and tag in (b'BLMD', b'ATST', b'LIT\0', b'2SID'):
                allowed &= struct.unpack_from('<I', parameter.body, 4)[0] == 0
            if parameter.kind == 0x11002 and tag == b'TEX\0':
                texture, _ = pstring(parameter.body, 4)
        allowed &= not bool(re.search(
            r'rock|cliff|mount|hill|terrain|landscap|tree|leaf|grass|water|ocean|cloud|shadow|glow',
            texture, re.I))
        result[name] = (allowed, texture, material)
    return result


def geometry(item, tree, materials, textures, inset):
    entity = next(n for n in walk(tree) if n.kind == 0x3f00000 and pstring(n.body, 0)[0] == item['entity'])
    mesh = next(n for n in entity.children if n.kind == 0x10000)
    positions, colours = [], []
    for group in (n for n in mesh.children if n.kind == 0x10002):
        name, offset = pstring(group.body, 4)
        allowed, texture, _ = materials[name]
        if not allowed or texture not in textures:
            return None
        topology, _, count, index_count, bones = struct.unpack_from('<5I', group.body, offset)
        parts = {n.kind: n for n in group.children}
        if bones or not {0x10005, 0x10008, 0x1000a} <= parts.keys():
            return None
        p = np.frombuffer(parts[0x10005].body, dtype='<f4', offset=4).reshape((-1, 3)).astype(np.float64)
        c = np.frombuffer(parts[0x10008].body, dtype=np.uint8, offset=4).reshape((-1, 4))
        if len(p) != count or len(c) != count or np.any(c[:, 3] != 255):
            return None
        ids = struct.unpack_from(f'<{index_count}I', parts[0x1000a].body, 4)
        faces = np.asarray(triangles(ids, topology), dtype=np.int32)
        if not len(faces):
            continue
        t = p[faces]
        normal = np.cross(t[:, 1]-t[:, 0], t[:, 2]-t[:, 0])
        length = np.linalg.norm(normal, axis=1)
        valid = length > 1e-8
        t, normal, length, faces = t[valid], normal[valid], length[valid], faces[valid]
        normal /= length[:, None]
        # Share coplanar face corners without averaging across creases. Rounding
        # the direction removes numerical differences between the same planes.
        normal = np.round(normal, 4)
        normal /= np.linalg.norm(normal, axis=1)[:, None]
        shifted = t-normal[:, None, :]*inset
        # Each face owns its vertices. The inward offset therefore cannot flip
        # winding or cancel at duplicated edges with opposing normals.
        assert np.all(np.einsum('ij,ij->i', np.cross(shifted[:, 1]-shifted[:, 0],
                                                   shifted[:, 2]-shifted[:, 0]), normal) > 0)
        assert np.allclose(np.linalg.norm(shifted-t, axis=2), inset)
        # P3D stores colors as BGRA. A distant proxy retains material average
        # albedo and prepared vertex shading; the original supplies near detail.
        albedo = textures[texture][[2, 1, 0]]/255
        rgb = np.clip(np.rint(c[faces, :3]*albedo), 0, 255).astype(np.uint8)
        rgba = np.concatenate((rgb, np.full((*rgb.shape[:2], 1), 255, dtype=np.uint8)), axis=2)
        positions.append(shifted.reshape((-1, 3)))
        colours.append(rgba.reshape((-1, 4)))
    if not positions:
        return None
    return np.concatenate(positions), np.concatenate(colours)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('art', type=Path)
    parser.add_argument('--level', choices=list('1234567'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--triangles', type=int, default=3000)
    parser.add_argument('--inset', type=float, default=.03)
    args = parser.parse_args()
    assert 1 <= args.triangles <= 20000 and .01 <= args.inset <= .1
    args.output.mkdir(exist_ok=False)
    paths = sorted(p for p in args.art.glob('*.p3d')
                   if re.fullmatch(r'l'+args.level+r'(?:[rz].*|_TERRA)\.p3d', p.name, re.I))
    hashes = {str(p): digest(p) for p in paths}
    trees = {p.name: nodes(p.read_bytes()) for p in paths}
    textures = {}
    for tree in trees.values():
        textures.update(texture_means(tree))
    pool = []
    for path in paths:
        if '_terra' not in path.name.lower():
            pool.extend(candidates(path))
    ranks, ranked = {}, []
    for item in sorted(pool, key=lambda i: (-i['score'], i['file'], i['entity'])):
        rank = ranks.get(item['file'], 0)
        ranks[item['file']] = rank+1
        ranked.append((rank, item))
    chosen, positions, colours, used = [], [], [], 0
    template = None
    for _, item in sorted(ranked, key=lambda pair: (pair[0], -pair[1]['score'])):
        tree = trees[item['file']]
        materials = material_data(tree)
        data = geometry(item, tree, materials, textures, args.inset)
        if data is None or used+len(data[0])//3 > args.triangles:
            continue
        used += len(data[0])//3
        positions.append(data[0]);colours.append(data[1]);chosen.append(item)
        if template is None:
            template = next(v[2] for v in materials.values() if v[0])
    assert chosen, 'No supported architecture fits the prototype budget'
    expanded = np.concatenate(positions).astype('<f4')
    expanded_colour = np.concatenate(colours)
    shared, shared_colour, ids, keys = [], [], [], {}
    for p, c in zip(expanded, expanded_colour):
        key = p.tobytes()+c.tobytes()
        if key not in keys:
            keys[key] = len(shared);shared.append(p);shared_colour.append(c)
        ids.append(keys[key])
    position = np.asarray(shared, dtype='<f4')
    colour = np.asarray(shared_colour, dtype=np.uint8)
    indices = np.asarray(ids, dtype='<u4')
    count, index_count = len(position), len(indices)
    assert count <= index_count <= 60000 and count == len(colour) and index_count % 3 == 0
    assert np.array_equal(position[indices], expanded)
    assert np.array_equal(colour[indices], expanded_colour)
    material_name = 'srr_far_material_l'+args.level
    material = copy.deepcopy(template)
    _, offset = pstring(material.body, 0)
    _, tail = pstring(material.body, offset+4)
    state = list(struct.unpack_from('<4I', material.body, tail))
    material.children = [n for n in material.children if n.kind != 0x11002]
    for n in material.children:
        if n.kind == 0x11005 and n.body[:4] in (b'DIFF', b'AMBI'):
            n.body = n.body[:4]+b'\xff'*4
    state[-1] = len(material.children)
    material.body = string(material_name)+struct.pack('<I', 0)+string('simple')+struct.pack('<4I', *state)
    group = Node(0x10002, struct.pack('<I', 0)+string(material_name)+
                 struct.pack('<5I', 0, 0x20, count, index_count, 0), [
        Node(0x10005, struct.pack('<I', count)+position.tobytes(), []),
        Node(0x10008, struct.pack('<I', count)+colour.tobytes(), []),
        Node(0x1000a, struct.pack('<I', index_count)+indices.tobytes(), [])])
    name = 'srr_far_l'+args.level
    low, high = position.min(axis=0), position.max(axis=0)
    center = (low+high)*.5
    radius = np.linalg.norm(position-center, axis=1).max()
    mesh = Node(0x10000, string(name)+struct.pack('<2I', 0, 1), [group,
        Node(0x10003, struct.pack('<6f', *low, *high), []),
        Node(0x10004, struct.pack('<4f', *center, radius), []),
        Node(0x10017, struct.pack('<I', 1), [])])
    entity = Node(0x3f00000, string(name)+struct.pack('<2I', 0, 1), [mesh])
    terra_name = next(n for n in trees if '_terra' in n.lower())
    tree = trees[terra_name]
    assert len(tree) == 1
    original_children = [n.encode() for n in tree[0].children]
    tree[0].children.extend((material, entity))
    output = args.output/terra_name
    output.write_bytes(tree[0].encode())
    reread = nodes(output.read_bytes())
    assert [n.encode() for n in reread[0].children[:-2]] == original_children
    assert all(digest(Path(p)) == sha for p, sha in hashes.items())
    report = dict(level=args.level, source_hashes=hashes, source_children_unchanged=True,
                  output=output.name, output_sha256=digest(output), entities=1, draws=1,
                  vertices=count, indices=index_count, shared_coplanar_vertices=True, triangles=used, inset_m=args.inset,
                  estimated_linear_mesh_bytes=count*36+index_count*2,
                  texture_count_added=0, selected=chosen,
                  status='PC prototype only; requires loader policy and physical visual/performance validation')
    (args.output/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('selected', 'source_hashes')}, indent=2))


if __name__ == '__main__':
    main()
