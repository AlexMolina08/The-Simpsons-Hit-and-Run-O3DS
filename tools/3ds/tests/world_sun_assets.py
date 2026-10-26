#!/usr/bin/env python3
"""Verify projected-world-lighting copies against original Pure3D assets."""
import argparse,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from prepare_character_detail import nodes
from skin_inventory import pstring

def walk(tree):
    for node in tree:
        yield node
        yield from walk(node.children)

def check(art,packed):
    manifest=json.loads((packed/'manifest.json').read_text())
    sources={p.name.lower():p for p in art.glob('*.p3d')}
    selected={(i['file'].lower(),i['mesh'],i['group']) for i in manifest['changes']}
    counts=dict(files=0,meshes=0,groups=0,vertices=0,changed_groups=len(selected))
    for path in packed.glob('*.p3d'):
        original=nodes(sources[path.name.lower()].read_bytes());updated=nodes(path.read_bytes())
        src={pstring(n.body,0)[0]:n for n in walk(original) if n.kind==0x10000}
        dst={pstring(n.body,0)[0]:n for n in walk(updated) if n.kind==0x10000}
        assert src.keys()==dst.keys()
        preserve_sort=manifest.get('sort_keys_preserved',False)
        if preserve_sort:
            assert [pstring(n.body,0)[0] for n in walk(original) if n.kind==0x11000]==[pstring(n.body,0)[0] for n in walk(updated) if n.kind==0x11000], 'Shader names/order/count changed'
        old_materials={pstring(n.body,0)[0]:n for n in walk(original) if n.kind==0x11000}
        new_materials={pstring(n.body,0)[0]:n for n in walk(updated) if n.kind==0x11000}
        for name,mesh in src.items():
            other=dst[name];assert mesh.body==other.body
            assert len(mesh.children)==len(other.children);gi=0
            for a,b in zip(mesh.children,other.children):
                assert a.kind==b.kind
                if a.kind!=0x10002:
                    assert a.encode()==b.encode();continue
                oldmat,off=pstring(a.body,4);newmat,new_off=pstring(b.body,4)
                assert a.body[:4]==b.body[:4] and a.body[off:]==b.body[new_off:]
                changed=(path.name.lower(),name,gi) in selected
                if preserve_sort:assert newmat==oldmat
                else:assert (newmat!=oldmat)==changed
                if changed:
                    a_shader,b_shader=old_materials[oldmat],new_materials[newmat]
                    _,old_off=pstring(a_shader.body,0);_,old_tail=pstring(a_shader.body,old_off+4)
                    _,new_off=pstring(b_shader.body,0);kind,new_tail=pstring(b_shader.body,new_off+4)
                    assert kind=='worldsun'
                    assert a_shader.body[old_off:old_off+4]==b_shader.body[new_off:new_off+4]
                    state=list(struct.unpack_from('<4I',a_shader.body,old_tail));state[-1]+=1
                    assert struct.pack('<4I',*state)==b_shader.body[new_tail:]
                    assert [n.encode() for n in a_shader.children]==[n.encode() for n in b_shader.children[:-1]],'Local material parameters changed'
                    extra=b_shader.children[-1];assert extra.kind==0x11002 and extra.body[:4]==b'REFL'
                    assert pstring(extra.body,4)[0]==manifest['texture']
                assert len(a.children)==len(b.children)
                for x,y in zip(a.children,b.children):
                    assert x.kind==y.kind
                    if x.kind!=0x10008 or changed:
                        assert x.encode()==y.encode(),(path.name,name,gi,hex(x.kind))
                    else:
                        assert x.body[:4]==y.body[:4] and x.body[7::4]==y.body[7::4]
                counts['vertices']+=struct.unpack_from('<I',a.body,off+8)[0]
                counts['groups']+=1;gi+=1
            counts['meshes']+=1
        oldtex=[n.encode() for n in walk(original) if n.kind==0x19000]
        newtex=[n.encode() for n in walk(updated) if n.kind==0x19000]
        for data in oldtex:assert data in newtex,'Original texture changed'
        counts['files']+=1
    return dict(level=manifest['level'],counts=counts,checks=['positions, normals, UVs, topology, indices and bounds exact','selected ground original RGBA exact','all original textures exact','no additional mesh or primitive group','local material parameters exact'])

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('art',type=Path);parser.add_argument('packed',type=Path,nargs='+')
    args=parser.parse_args();print(json.dumps([check(args.art,path) for path in args.packed],indent=2))
if __name__=='__main__':main()
