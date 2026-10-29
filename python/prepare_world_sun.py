#!/usr/bin/env python3
"""Prototype a level-wide projected sunlight texture; originals remain read-only."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import sys

import numpy as np
from PIL import Image

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools/3ds'))
from prepare_scene_occlusion import collect, walk, sun_direction
from prepare_character_detail import triangles
from skin_inventory import chunks, pstring


def ground_groups(path):
    data = path.read_bytes()
    materials = {}
    for kind, body, child, stop in walk(data):
        if kind != 0x11000:
            continue
        name, offset = pstring(data, body)
        shader, _ = pstring(data, offset + 4)
        allowed, texture = shader == 'simple', ''
        for k, b, c, e in chunks(data, child, stop):
            tag = data[b:b+4]
            if k == 0x11003 and tag in (b'BLMD', b'ATST', b'LIT\0'):
                allowed &= struct.unpack_from('<I', data, b+4)[0] == 0
            if k == 0x11002 and tag == b'TEX\0':
                texture, _ = pstring(data, b+4)
        materials[name] = allowed and bool(re.search(r'grass|sidew|sdewl|asphal|^rd\d|driveway|dirt|concret|road|soil|ground|pavem', texture, re.I))
    result = []
    for kind, body, child, stop in walk(data):
        if kind != 0x10000:
            continue
        mesh, _ = pstring(data, body)
        if re.search(r'cloud|sky|ocean|shadow|wheel|girl|boy|head|skirt|fan', mesh, re.I):
            continue
        group_index = 0
        for k, b, c, e in chunks(data, child, stop):
            if k != 0x10002:
                continue
            index, group_index = group_index, group_index + 1
            material, offset = pstring(data, b+4)
            topology, fmt, count, index_count, bones = struct.unpack_from('<5I', data, offset)
            if topology not in (0, 1) or bones or not materials.get(material):
                continue
            parts = {kk: bb for kk, bb, cc, ee in chunks(data, c, e)}
            if not {0x10005, 0x10008, 0x1000A} <= parts.keys():
                continue
            points = np.frombuffer(data, dtype='<f4', count=3*count, offset=parts[0x10005]+4).reshape((-1, 3)).astype(np.float64)
            centre = (points.min(axis=0)+points.max(axis=0))*.5
            if abs(centre).max() < 50 and np.ptp(points, axis=0).max() < 20:
                continue
            ids = struct.unpack_from(f'<{index_count}I', data, parts[0x1000A]+4)
            faces = np.asarray(triangles(ids, topology), dtype=np.int32)
            if not len(faces):
                continue
            cross = np.cross(points[faces[:, 1]]-points[faces[:, 0]], points[faces[:, 2]]-points[faces[:, 0]])
            length = np.linalg.norm(cross, axis=1)
            up = (length > 1e-8) & (cross[:, 1] > .4*length)
            if length[up].sum() < .85*length.sum():
                continue
            normals = cross[up]/length[up, None]
            result.append(dict(mesh=mesh, group=index, material=material, points=points, faces=faces[up], normals=normals))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets', type=Path)
    parser.add_argument('--level', choices=list('1234567'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--helper', type=Path, required=True)
    parser.add_argument('--size', type=int, default=1024)
    parser.add_argument('--sun-strength', type=float, default=None)
    parser.add_argument('--contact-strength', type=float, default=0.0, help='Scale the helper contact shade; zero preserves sunlight-only output')
    args = parser.parse_args()
    assert args.size in (256, 512, 1024)
    assert 0 <= args.contact_strength <= 1
    assert args.sun_strength is None or 0 <= args.sun_strength <= .5
    args.output.mkdir(exist_ok=False)
    paths = sorted(p for p in args.assets.glob('*.p3d') if re.fullmatch(r'l'+args.level+r'[rz].*\.p3d', p.name, re.I))
    selected = {p.name: ground_groups(p) for p in paths}
    groups = [g for values in selected.values() for g in values]
    assert groups
    points = np.concatenate([g['points'][g['faces'].reshape(-1)] for g in groups])
    low, high = points[:, (0, 2)].min(axis=0)-2, points[:, (0, 2)].max(axis=0)+2
    span = high-low
    size = args.size
    heights = np.full((size, size), -np.inf, dtype=np.float32)
    normals = np.zeros((size, size, 3), dtype=np.float32)
    for group in groups:
        for face, normal in zip(group['faces'], group['normals']):
            triangle = group['points'][face]
            uv = (triangle[:, (0, 2)]-low)/span*size-.5
            begin = np.maximum(0, np.ceil(uv.min(axis=0)).astype(int))
            end = np.minimum(size-1, np.floor(uv.max(axis=0)).astype(int))
            if np.any(begin > end):
                continue
            x, z = np.meshgrid(np.arange(begin[0], end[0]+1), np.arange(begin[1], end[1]+1))
            a, b, c = uv
            denominator = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(denominator) < 1e-12:
                continue
            wa = ((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(z-c[1]))/denominator
            wb = ((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(z-c[1]))/denominator
            wc = 1-wa-wb
            y = wa*triangle[0, 1]+wb*triangle[1, 1]+wc*triangle[2, 1]
            region = np.s_[begin[1]:end[1]+1, begin[0]:end[0]+1]
            update = (wa >= -1e-8) & (wb >= -1e-8) & (wc >= -1e-8) & (y > heights[region])
            heights[region][update] = y[update]
            normals[region][update] = normal
    valid = np.isfinite(heights)
    rows, columns = np.nonzero(valid)
    queries = np.column_stack((low[0]+(columns+.5)/size*span[0], heights[valid], low[1]+(rows+.5)/size*span[1], normals[valid])).astype('<f4')
    sources = [collect(p) for p in paths]
    positions, faces = [], []
    for source in sources:
        faces.extend([[i+len(positions) for i in triangle] for triangle in source['indices']])
        positions.extend(source['positions'])
    geometry = np.column_stack((np.asarray(positions), np.zeros((len(positions), 3)))).astype('<f4')
    indices = np.asarray(faces, dtype='<u4').tobytes()
    visibility = bytearray()
    contact = bytearray()
    capacity = 999999-len(geometry)
    assert capacity > 0
    for start in range(0, len(queries), capacity):
        batch = queries[start:start+capacity]
        request = struct.pack('<IIIf', len(geometry)+len(batch), len(faces), 8, 80.)+geometry.tobytes()+batch.tobytes()+indices
        result = subprocess.run([str(args.helper.resolve()), '--sun', *(str(x) for x in sun_direction(args.level))], input=request, capture_output=True, check=True).stdout
        assert len(result) == len(geometry)+len(batch)
        visibility.extend(result[len(geometry):])
        if args.contact_strength:
            request=struct.pack('<IIIf', len(geometry)+len(batch), len(faces), 16, 2.25)+geometry.tobytes()+batch.tobytes()+indices
            result=subprocess.run([str(args.helper.resolve())],input=request,capture_output=True,check=True).stdout
            assert len(result)==len(geometry)+len(batch)
            contact.extend(result[len(geometry):])
    pixels = np.full((size, size), 255, dtype=np.uint8)
    strength = args.sun_strength if args.sun_strength is not None else (.04 if args.level in '467' else .22)
    shade=255-strength*(255-np.frombuffer(visibility,dtype=np.uint8))
    if args.contact_strength:
        shade*=1-args.contact_strength*(1-np.frombuffer(contact,dtype=np.uint8).astype(np.float64)/255)
    pixels[valid]=np.rint(shade).astype(np.uint8)
    Image.fromarray(pixels).convert('RGB').save(args.output/'sunlight.png')
    Image.fromarray((valid*255).astype(np.uint8)).save(args.output/'coverage.png')
    report = dict(level=args.level, size=size, sun_strength=strength, contact_strength=args.contact_strength, xz_low=low.tolist(), xz_high=high.tolist(),
                  uv_parameters=[1/span[0], 1/span[1], -low[0]/span[0], -low[1]/span[1]],
                  ground_pixels=len(queries), shadowed_pixels=int((pixels < 255).sum()), minimum=int(pixels.min()),
                  metres_per_pixel=(span/size).tolist(), top_surface_only=True, alpha_foliage_occluders=False,
                  files=[dict(file=p.name, source_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),
                              groups=[{k: g[k] for k in ('mesh', 'group', 'material')} for g in selected[p.name]]) for p in paths])
    (args.output/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'files'}))


if __name__ == '__main__':
    main()
