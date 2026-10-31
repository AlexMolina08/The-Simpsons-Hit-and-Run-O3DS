#!/usr/bin/env python3
"""Check exact-once ground relocation, scoped dependencies and source protection."""
from collections import Counter
from pathlib import Path
import struct
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from prepare_world_residency import (prepare, root, name, groups, pstring,
    geometry_signature, exterior_files, STATIC, INTERSECT, SHADER, TEXTURE, GROUP, MESH, Node)


def text(value):
    value = value.encode()
    return bytes([len(value)]) + value


def shader():
    return Node(SHADER, text('road') + struct.pack('<I', 0) + text('simple') + bytes(16),
                [Node(0x11002, b'TEX\0' + text('asphalt'), [])])


def texture(payload):
    return Node(TEXTURE, text('asphalt') + struct.pack('<9I', 0, 8, 8, 32, 0, 1, 0, 0, 0),
                [Node(0x19001, payload, [])])


def entity(alpha=0):
    group = Node(GROUP, struct.pack('<I', 0) + text('road') + struct.pack('<5I', 0, 0, 3, 3, 0),
                 [Node(0x10005, struct.pack('<I9f', 3, 0, 0, 0, 1, 0, 0, 0, 0, 1), []),
                  Node(0x1000a, struct.pack('<4I', 3, 0, 1, 2), [])])
    return Node(STATIC, text('trackShape_1') + struct.pack('<2I', 0, alpha),
                [Node(MESH, text('mesh') + bytes(8), [group])])


def file(children):
    return Node(0xff443350, b'', children).encode()


with tempfile.TemporaryDirectory(prefix='world-residency-') as directory:
    folder = Path(directory)
    art = folder / 'source'
    art.mkdir()
    (art / 'l2_TERRA.p3d').write_bytes(file([]))
    interactive = Node(0x3f00002, b'persistent object identity must stay exact', [])
    collision = Node(INTERSECT, b'exact collision bytes', [])
    blocks = [texture(b'image A'), shader(), interactive, collision, entity(), entity(1)]
    (art / 'l2r1.p3d').write_bytes(file(blocks))
    # Same file-local material names resolve to different image data. The two
    # permanent aliases must keep their own texture, independent of load order.
    (art / 'l2r2.p3d').write_bytes(file([texture(b'image B'), shader(), collision, entity()]))
    before = {p.name: p.read_bytes() for p in art.iterdir()}
    output = folder / 'candidate'
    report = prepare(art, None, 2, output, landmarks=0)
    assert report['road_meshes'] == 2 and report['intersects'] == 2
    assert {p.name: p.read_bytes() for p in art.iterdir()} == before
    permanent = root((output / 'art/l2_TERRA.p3d').read_bytes())
    moved = [node for node in permanent.children if node.kind == STATIC]
    assert len({name(node) for node in moved}) == 2
    assert all(geometry_signature(node) == geometry_signature(entity()) for node in moved)
    assert Counter(node.encode() for node in permanent.children if node.kind == INTERSECT) == Counter([collision.encode()] * 2)
    shaders = {name(node): node for node in permanent.children if node.kind == SHADER}
    textures = {name(node): node for node in permanent.children if node.kind == TEXTURE}
    assert len(shaders) == len(textures) == 2
    assert {tuple(child.encode() for child in node.children) for node in textures.values()} == {
        tuple(child.encode() for child in texture(payload).children) for payload in (b'image A', b'image B')}
    for moved_entity in moved:
        material = pstring(groups(moved_entity)[0].body, 4)[0]
        texture_name = pstring(shaders[material].children[0].body, 4)[0]
        assert texture_name in textures
    remaining = root((output / 'art/l2r1.p3d').read_bytes()).children
    assert [node.encode() for node in remaining] == [node.encode() for node in blocks if node.kind != INTERSECT and (node.kind != STATIC or node.body == entity(1).body)]
    assert report['rejected'][0]['reason'] == 'Translucent static entity'
    try:
        prepare(art, None, 2, output, landmarks=0)
    except ValueError:
        pass
    else:
        raise AssertionError('Existing output overwritten')
    for budget in (0, 1):
        target = folder / f'budget-{budget}'
        try:
            prepare(art, None, 2, target, landmarks=0, max_linear_bytes=budget)
        except ValueError:
            assert not target.exists()
        else:
            raise AssertionError('Invalid or exhausted memory budget accepted')
    # Conflicting material definitions must retain the affected surface in its
    # original streaming file, rather than guessing which material to publish.
    ambiguous = shader()
    ambiguous.body += b'conflict'
    (art / 'l2r2.p3d').write_bytes(file([texture(b'image B'), shader(), ambiguous, collision, entity()]))
    conflict = prepare(art, None, 2, folder / 'conflict', landmarks=0)
    assert conflict['road_meshes'] == 1 and len(conflict['rejected']) == 2
    assert [node.encode() for node in root((folder / 'conflict/art/l2r2.p3d').read_bytes()).children if node.kind == STATIC] == [entity().encode()]
    # A permanent list has a different lifetime and capacity from a sector list.
    (art / 'l2r1.p3d').write_bytes(file([texture(b'image A'), shader(), entity()] + [collision] * 1536))
    try:
        prepare(art, None, 2, folder / 'overflow', landmarks=0)
    except ValueError as error:
        assert 'capacity' in str(error) and not (folder / 'overflow').exists()
    else:
        raise AssertionError('Permanent section overflow accepted')

# Whole-scene selection adds short opaque pieces without adding foliage or alpha.
with tempfile.TemporaryDirectory(prefix='opaque-residency-') as directory:
    from copy import deepcopy
    from prepare_world_residency import replace_text
    from prepare_stream_probes import uid
    import hashlib
    folder=Path(directory);art=folder/'art';art.mkdir()
    road=entity();road.children[0].children += [Node(0x10004,struct.pack('<4f',1,2,3,4),[])]
    wall=deepcopy(road);wall.body=replace_text(wall.body,0,'short_wall')
    leaf=deepcopy(wall);leaf.body=replace_text(leaf.body,0,'leaf_wall')
    groups(leaf)[0].body=replace_text(groups(leaf)[0].body,4,'leaf_shader')
    leaf_shader=shader();leaf_shader.body=replace_text(leaf_shader.body,0,'leaf_shader')
    leaf_shader.children[0].body=b'TEX\0'+text('tree_leaf')
    leaf_texture=texture(b'leaves');leaf_texture.body=replace_text(leaf_texture.body,0,'tree_leaf')
    (art/'l2_TERRA.p3d').write_bytes(file([]))
    for filename in exterior_files(2):
        if not (art/filename).exists():
            (art/filename).write_bytes(file([]))
    source=[texture(b'wall'),shader(),leaf_texture,leaf_shader,road,wall,leaf,entity(1),Node(INTERSECT,b'ground',[])]
    (art/'l2r1.p3d').write_bytes(file(source))
    report=prepare(art,None,2,folder/'candidate',landmarks=0,opaque_world=True)
    assert report['resident_meshes']==2 and report['landmarks']==0
    assert {r['entity'] for r in report['aliases'].values()}=={'short_wall','trackShape_1'}
    data=(folder/'candidate/resident-visibility.bin').read_bytes()
    magic,version,count,stride,terrain,level=struct.unpack_from('<4sIIIQQ',data)
    assert (magic,version,count,stride,terrain,level)==(b'WRV1',1,2,32,uid('art\\l2_TERRA.p3d'),2)
    assert hashlib.sha256(data[64:]).digest()==data[32:64]
    records=list(struct.iter_unpack('<QQ4f',data[64:]));assert records==sorted(records)
    assert {r[0] for r in records}=={uid(alias) for alias in report['aliases']}
    assert all(r[1:]==(uid('l2r1.p3d'),1.,2.,3.,4.) for r in records)
    remain=root((folder/'candidate/art/l2r1.p3d').read_bytes())
    assert [n.encode() for n in remain.children if n.kind==STATIC]==[leaf.encode(),entity(1).encode()]

with tempfile.TemporaryDirectory(prefix='resident-png-aliases-') as directory:
    import importlib
    import io
    from copy import deepcopy
    from PIL import Image
    sys.path.insert(0, str(Path(__file__).resolve().parents[3]/'browser-builder/engine-conversion'))
    semantic = importlib.import_module('asset-semantics').semantic_fingerprint
    folder = Path(directory)
    art = folder/'native'
    variant = folder/'browser'
    art.mkdir()
    variant.mkdir()

    def png_texture(compression, colour):
        stream = io.BytesIO()
        Image.new('RGBA', (8, 8), colour).save(stream, format='PNG', compress_level=compression)
        encoded = stream.getvalue()
        result = texture(b'')
        result.children[0].children = [Node(0x19002, struct.pack('<I', len(encoded))+encoded, [])]
        return result

    for target, compression in ((art, 0), (variant, 9)):
        (target/'l2_TERRA.p3d').write_bytes(file([png_texture(compression, 'red')]))
        for index, colour in ((1, 'red'), (2, 'blue')):
            (target/f'l2r{index}.p3d').write_bytes(file([
                png_texture(1 if target == variant else compression, colour), shader(), collision, entity()]))
    accepted = prepare(art, None, 2, folder/'accepted', landmarks=0, resource_fingerprint=semantic)
    plain = prepare(art, None, 2, folder/'plain', landmarks=0)
    references = accepted['resource_references']
    assert references['l2r1.p3d']['road']['textures']['asphalt']['permanent']
    assert not references['l2r2.p3d']['road']['textures']['asphalt']['permanent']
    prepared = prepare(variant, None, 2, folder/'variant', landmarks=0,
                       resource_references=references, resource_fingerprint=semantic)
    assert prepared['resource_references'] == references
    for path in (folder/'accepted/art').iterdir():
        assert path.read_bytes() == (folder/'plain/art'/path.name).read_bytes()
        assert semantic(path.read_bytes()) == semantic((folder/'variant/art'/path.name).read_bytes())
    for scenario in ('missing', 'extra', 'shader', 'pixels', 'permanent'):
        changed = deepcopy(references)
        if scenario == 'missing':
            del changed['l2r1.p3d']['road']
        elif scenario == 'extra':
            changed['unused.p3d'] = {}
        elif scenario == 'shader':
            changed['l2r1.p3d']['road']['shader_sha256'] = '0'*64
        elif scenario == 'pixels':
            changed['l2r1.p3d']['road']['textures']['asphalt']['semantic_sha256'] = '0'*64
        else:
            changed['l2r2.p3d']['road']['textures']['asphalt']['permanent'] = True
        output = folder/scenario
        try:
            prepare(variant, None, 2, output, landmarks=0,
                    resource_references=changed, resource_fingerprint=semantic)
        except ValueError:
            assert not output.exists()
        else:
            raise AssertionError('Unrecognized resource references accepted: '+scenario)

print('PASS: exact-once collision/geometry, scoped materials, original protection, capacity rejection, lossless PNG aliases and strict resource-reference rejection.')
