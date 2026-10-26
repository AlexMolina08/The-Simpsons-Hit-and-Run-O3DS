#!/usr/bin/env python3
"""Pack a private world-sun prototype while retaining original ground geometry."""
from pathlib import Path
import argparse, copy, hashlib, io, json, re, struct, sys
from PIL import Image
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'tools/3ds'))
from prepare_character_detail import Node,nodes
from skin_inventory import pstring

def walk(ns):
    for node in ns:
        yield node
        yield from walk(node.children)
def string(value):
    data=value.encode()+b'\0';data+=b'\0'*((-len(data))%4)
    assert len(data)<256
    return bytes([len(data)])+data

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('art',type=Path)
    parser.add_argument('--baked',type=Path,required=True)
    parser.add_argument('--map',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--preserve-sort',action='store_true',help='Keep original shader names, order and count; skip materials shared with unsupported groups')
    parser.add_argument('--size',type=int,choices=[256,512,1024],default=None)
    args=parser.parse_args();report=json.loads((args.map/'manifest.json').read_text());level=report['level']
    args.output.mkdir(exist_ok=False)
    paths={p.name.lower():p for p in args.art.glob('*.p3d')}
    texture_name='srr_sun_l'+level
    clones={};changes=[];skipped=[]
    for entry in report['files']:
        if not entry['groups']:continue
        source=paths[entry['file'].lower()];original=source.read_bytes()
        assert hashlib.sha256(original).hexdigest()==entry['source_sha256']
        original_nodes=nodes(original)
        original_meshes={pstring(n.body,0)[0]:n for n in walk(original_nodes) if n.kind==0x10000}
        local_materials={pstring(n.body,0)[0]:n for n in walk(original_nodes) if n.kind==0x11000}
        usage={}
        for mesh_name,mesh in original_meshes.items():
            for gi,g in enumerate(n for n in mesh.children if n.kind==0x10002):
                material,_=pstring(g.body,4);usage.setdefault(material,set()).add((mesh_name,gi))
        selection={}
        for item in entry['groups']:selection.setdefault(item['material'],set()).add((item['mesh'],item['group']))
        safe={name for name,groups in selection.items() if groups==usage[name]}
        baked=args.baked/source.name.lower();tree=nodes(baked.read_bytes())
        meshes={pstring(n.body,0)[0]:n for n in walk(tree) if n.kind==0x10000}
        output_materials={pstring(n.body,0)[0]:n for n in walk(tree) if n.kind==0x11000}
        needed={}
        for item in entry['groups']:
            if args.preserve_sort and item['material'] not in safe:
                skipped.append(dict(file=source.name,**item));continue
            mesh=meshes[item['mesh']];group=[n for n in mesh.children if n.kind==0x10002][item['group']]
            before=copy.deepcopy(group);material,offset=pstring(group.body,4);assert material==item['material']
            source_material=local_materials[material]
            digest=hashlib.sha256(source_material.encode()).hexdigest()
            clone_key=(source.name,material) if args.preserve_sort else digest
            clone_name=material if args.preserve_sort else 'sun'+level+'_'+digest[:12]
            if clone_key not in clones:
                clone=copy.deepcopy(source_material);_,off=pstring(clone.body,0);_,tail=pstring(clone.body,off+4)
                state=list(struct.unpack_from('<4I',clone.body,tail));state[-1]+=1
                clone.body=string(clone_name)+clone.body[off:off+4]+string('worldsun')+struct.pack('<4I',*state)
                clone.children.append(Node(0x11002,b'REFL'+string(texture_name),[]));clones[clone_key]=clone
            needed[clone_key]=clones[clone_key]
            group.body=group.body[:4]+string(clone_name)+group.body[offset:]
            # Projected ground shadows replace the coarse per-vertex sunlight.
            original_group=[n for n in original_meshes[item['mesh']].children if n.kind==0x10002][item['group']]
            original_colour=next(n for n in original_group.children if n.kind==0x10008)
            for i,node in enumerate(group.children):
                if node.kind==0x10008:group.children[i]=copy.deepcopy(original_colour)
            assert len(group.children)==len(before.children)
            for a,b in zip(group.children,before.children):
                if a.kind!=0x10008:assert a.encode()==b.encode()
            changes.append(dict(file=source.name,mesh=item['mesh'],group=item['group'],material=material,clone=clone_name))
        assert len(tree)==1
        if args.preserve_sort:
            for clone in needed.values():
                name,_=pstring(clone.body,0);target=output_materials[name]
                target.body=clone.body;target.children=copy.deepcopy(clone.children)
        else:
            tree[0].children=[copy.deepcopy(n) for n in needed.values()]+tree[0].children
        encoded=tree[0].encode();assert nodes(encoded)[0].encode()==encoded
        (args.output/source.name.lower()).write_bytes(encoded)
    # One level-resident 1024-square L8 texture, loaded before streamed zones.
    terra=paths['l'+level+'_terra.p3d'];tree=nodes(terra.read_bytes());size=args.size or report['size']
    assert size<=report['size']
    png=(args.map/'sunlight.png').read_bytes()
    if size!=report['size']:
        stream=io.BytesIO();Image.open(io.BytesIO(png)).resize((size,size),Image.Resampling.LANCZOS).save(stream,format='PNG');png=stream.getvalue()
    image=Node(0x19001,string(texture_name)+struct.pack('<7I',14000,size,size,32,0,0,1),[Node(0x19002,struct.pack('<I',len(png))+png,[])])
    low,high=report['xz_low'],report['xz_high'];span=[high[i]-low[i] for i in range(2)]
    # The PNG decoder writes TOP rows directly; the PICA upload does not flip.
    parameters=[1/span[0],1/span[1],-low[0]/span[0],-low[1]/span[1]]
    metadata=Node(0x7f53554e,struct.pack('<I4f',1,*parameters),[])
    texture=Node(0x19000,string(texture_name)+struct.pack('<9I',14000,size,size,32,0,1,0,3,0),[metadata,image])
    tree[0].children.insert(0,texture);runtime_terra=('L' if level in '147' else 'l')+level+'_TERRA.p3d'
    (args.output/runtime_terra).write_bytes(tree[0].encode())
    result=dict(sort_keys_preserved=args.preserve_sort,skipped=skipped,level=level,texture=texture_name,texture_bytes=size*size,uv_parameters=parameters,changes=changes,geometry_unchanged=True,alpha_unchanged=True,original_textures_unchanged=True)
    (args.output/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='changes'}));print('groups',len(changes),'materials',len(clones))
if __name__=='__main__':main()
