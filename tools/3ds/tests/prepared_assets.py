#!/usr/bin/env python3
"""Check derived scenery and pedestrian assets against their untouched sources."""
import argparse
import hashlib
import json
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from skin_inventory import chunks, pstring


def tree(data, start=0, end=None):
    return [(kind, data[body:child], tree(data, child, stop))
            for kind, body, child, stop in chunks(data, start, len(data) if end is None else end)]


def scene(source, derived):
    original, output = source.read_bytes(), derived.read_bytes()
    assert len(original) == len(output)
    changed = 0

    def compare(a, b):
        nonlocal changed
        assert len(a) == len(b)
        for (kind, body, children), (other, replacement, descendants) in zip(a, b):
            assert kind == other
            if kind == 0x10008:
                assert body[:4] == replacement[:4]
                assert body[7::4] == replacement[7::4], 'alpha changed'
                for i, (before, after) in enumerate(zip(body[4:], replacement[4:])):
                    if i % 4 != 3:
                        assert before >= after >= before*.69 - 1
                        changed += before != after
            else:
                assert body == replacement, f'non-color chunk changed: {kind:x}'
            compare(children, descendants)

    compare(tree(original), tree(output))
    assert changed
    return changed


def character(source, derived, vertex_budget=64):
    added = 0

    def compare(a, b):
        nonlocal added
        assert len(a) == len(b)
        for (kind, body, children), (other, replacement, descendants) in zip(a, b):
            assert kind == other
            if kind == 0x10001:
                def points(groups):
                    return [p for k,body,parts in groups if k==0x10002
                            for kk,data,unused in parts if kk==0x10005
                            for p in struct.iter_unpack('<3f',data[4:])]
                original_points,derived_points=points(children),points(descendants)
                for axis in range(3):
                    assert min(p[axis] for p in derived_points)>=min(p[axis] for p in original_points)-1e-6
                    assert max(p[axis] for p in derived_points)<=max(p[axis] for p in original_points)+1e-6
                spheres=[data for k,data,unused in children if k==0x10004]
                if spheres:
                    sphere=struct.unpack('<4f',spheres[0])
                    before=max(math.sqrt(sum((p[i]-sphere[i])**2 for i in range(3))) for p in original_points)
                    after=max(math.sqrt(sum((p[i]-sphere[i])**2 for i in range(3))) for p in derived_points)
                    assert after<=max(before,sphere[3])+1e-5, 'refinement exceeds the original culling sphere'
            if kind != 0x10002 or body == replacement:
                assert body == replacement
                compare(children, descendants)
                continue
            material, offset = pstring(body, 4)
            assert replacement[:offset] == body[:offset]
            topology, fmt, count, index_count, bones = struct.unpack_from('<5I', body, offset)
            new_topology, new_fmt, new_count, new_indices, new_bones = struct.unpack_from('<5I', replacement, offset)
            assert new_topology in (0,1) and fmt == new_fmt and bones == new_bones
            assert not fmt & 256, 'weighted skin changed'
            assert 0 < new_count-count <= vertex_budget
            added += new_count-count
            old = {k: b for k, b, c in children}
            new = {k: b for k, b, c in descendants}
            assert set(new) == set(old)-{0x10010}
            assert 0x1000C not in old, 'weights changed'
            for channel, stride, header in ((0x10005,12,4), (0x10006,12,4),
                                            (0x10007,8,8), (0x1000B,4,4), (0x10008,4,4)):
                if channel not in old:
                    continue
                assert struct.unpack_from('<I',new[channel])[0] == new_count
                assert new[channel][4:header] == old[channel][4:header]
                assert new[channel][header:header+count*stride] == old[channel][header:]
                assert len(new[channel]) == header+new_count*stride
            for channel in set(new)-{0x10005,0x10006,0x10007,0x1000B,0x10008,0x1000A}:
                assert new[channel] == old[channel], 'palette or unrelated data changed'
            positions = list(struct.iter_unpack('<3f',new[0x10005][4:]))
            normals = list(struct.iter_unpack('<3f',new[0x10006][4:]))
            original_indices = struct.unpack_from(f'<{index_count}I',old[0x1000A],4)
            faces = []
            for i in range(0,len(original_indices)-2,3 if topology == 0 else 1):
                face = list(original_indices[i:i+3])
                if topology == 1 and i % 2:
                    face[0],face[1] = face[1],face[0]
                if len(set(face)) == 3:
                    faces.append(face)
            edges = {tuple(sorted((face[i],face[(i+1)%3]))) for face in faces for i in range(3)}
            parents = {i:[{i}] for i in range(count)}
            # New vertices can use the original skin's broader bounds across
            # material groups, so check finiteness and maximum curvature here.
            for i, position in enumerate(positions[count:], count):
                assert all(math.isfinite(x) for x in position)
                assert abs(sum(x*x for x in normals[i])-1) < 1e-5
                bone = new[0x1000B][4+4*i:8+4*i]
                uv = struct.unpack_from('<2f',new[0x10007],8+8*i)
                pairs = []
                for j,k in edges:
                    if old[0x1000B][4+4*j:8+4*j] != bone:
                        continue
                    if old[0x1000B][4+4*k:8+4*k] != bone:
                        continue
                    midpoint = [(positions[j][a]+positions[k][a])*.5 for a in range(3)]
                    if sum((position[a]-midpoint[a])**2 for a in range(3)) > .02001**2:
                        continue
                    first = struct.unpack_from('<2f',old[0x10007],8+8*j)
                    second = struct.unpack_from('<2f',old[0x10007],8+8*k)
                    if all(abs(uv[a]-(first[a]+second[a])*.5)<1e-6 for a in range(2)):
                        pairs.append({j,k})
                assert pairs, 'new vertex is not an original same-bone edge midpoint'
                parents[i] = pairs
            indices = struct.unpack_from(f'<{new_indices}I',new[0x1000A],4)
            assert len(new[0x1000A]) == 4+4*new_indices and max(indices) < new_count
            if new_topology==0:assert new_indices % 3 == 0
            assert set(range(count,new_count)) <= set(indices), 'unused detail vertex'
            def cross(face):
                a,b,c = [positions[i] for i in face]
                u,v = [b[i]-a[i] for i in range(3)],[c[i]-a[i] for i in range(3)]
                return (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
            for i in range(0,len(indices)-2,3 if new_topology==0 else 1):
                face=list(indices[i:i+3])
                if new_topology==1 and i%2:face[0],face[1]=face[1],face[0]
                if len(set(face))<3:continue
                candidates = [original for original in faces
                              if all(any(parent <= set(original) for parent in parents[v]) for v in face)]
                assert candidates, 'refined triangle crosses an original face boundary'
                normal = cross(face)
                assert any(sum(a*b for a,b in zip(normal,cross(original))) >= -1e-12
                           for original in candidates), 'refined triangle reverses original winding'

    compare(tree(source.read_bytes()), tree(derived.read_bytes()))
    assert 0 < added <= vertex_budget
    return added


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('art',type=Path)
    parser.add_argument('campaign',type=Path)
    parser.add_argument('--scenery',action='append',help='Prepared scenery directory name; repeat for multiple sets')
    parser.add_argument('--characters',default='refined-characters',help='Prepared pedestrian directory name')
    parser.add_argument('--character-vertex-budget',type=int,choices=[64,128],default=64)
    args = parser.parse_args()
    characters = args.campaign/args.characters
    vertices = 0
    for record in json.loads((characters/'manifest.json').read_text()):
        source, derived = args.art/'chars'/record['file'], characters/record['file']
        assert hashlib.sha256(source.read_bytes()).hexdigest() == record['source_sha256']
        assert hashlib.sha256(derived.read_bytes()).hexdigest() == record['output_sha256']
        vertices += character(source,derived,args.character_vertex_budget)
    files, channels = 0, 0
    for folder in args.scenery or ('prepared-scenery','prepared-scenery-56'):
        directory = args.campaign/folder
        for record in json.loads((directory/'manifest.json').read_text()):
            source, derived = args.art/record['file'],directory/record['file']
            assert hashlib.sha256(source.read_bytes()).hexdigest() == record['source_sha256']
            assert hashlib.sha256(derived.read_bytes()).hexdigest() == record['output_sha256']
            channels += scene(source,derived)
            files += 1
    print(f'PASS: {vertices} added rigid pedestrian vertices; original vertices, animation data, palettes and textures preserved. '
          f'{files} scenery files change only {channels} existing RGB bytes; exact alpha, geometry, materials and sizes. Not hardware proof.')


if __name__ == '__main__':
    main()
