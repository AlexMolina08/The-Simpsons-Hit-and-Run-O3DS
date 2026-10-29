#!/usr/bin/env python3
"""Add private windshield reflection materials using each car's existing map."""
import argparse,copy,hashlib,json,re,struct
from pathlib import Path
from prepare_character_detail import Node,nodes
from skin_inventory import pstring

def walk(tree):
    for node in tree:
        yield node
        yield from walk(node.children)
def string(value):
    b=value.encode()+b'\0';b+=b'\0'*((-len(b))%4)
    return bytes([len(b)])+b

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cars',type=Path);parser.add_argument('--models',nargs='+',required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--fallback-map',help='Existing per-level reflection texture for cars without an authored reflection material')
    args=parser.parse_args();args.output.mkdir(exist_ok=False);report=[]
    paths={p.stem.lower():p for p in args.cars.glob('*.p3d')}
    for model in args.models:
        path=paths[model.lower()];original=path.read_bytes();tree=nodes(original)
        materials={pstring(n.body,0)[0]:n for n in walk(tree) if n.kind==0x11000}
        reflect=[]
        for name,n in materials.items():
            _,off=pstring(n.body,0);kind,_=pstring(n.body,off+4)
            if kind in ('spheremap','environment'):
                reflect.extend(pstring(p.body,4)[0] for p in n.children if p.kind==0x11002 and p.body[:4]==b'REFL')
        if not reflect:
            if not args.fallback_map:continue
            reflect=[args.fallback_map]
        counts={};eligible={}
        for n in walk(tree):
            if n.kind==0x10002:
                mat,_=pstring(n.body,4);counts[mat]=counts.get(mat,0)+1
                eligible[mat]=eligible.get(mat,True) and any(p.kind==0x10006 for p in n.children)
        changed=[]
        for name,n in materials.items():
            _,off=pstring(n.body,0);kind,tail=pstring(n.body,off+4)
            if kind!='simple' or not re.search('windsh|windshe',name,re.I) or not eligible.get(name):continue
            blend=next((struct.unpack_from('<I',p.body,4)[0] for p in n.children if p.kind==0x11003 and p.body[:4]==b'BLMD'),0)
            if blend not in (0,1):continue
            assert not any(p.body[:4] in (b'REFL',b'ENVB') for p in n.children)
            state=list(struct.unpack_from('<4I',n.body,tail));state[-1]+=2
            n.body=n.body[:off+4]+string('vehicle_glass')+struct.pack('<4I',*state)
            n.children.extend([Node(0x11002,b'REFL'+string(reflect[0]),[]),Node(0x11005,b'ENVB'+bytes([48,48,48,255]),[])])
            changed.append(dict(material=name,groups=counts[name],reflection_map=reflect[0],strength=48,blend_mode=blend))
        if not changed:continue
        encoded=tree[0].encode();(args.output/path.name).write_bytes(encoded)
        report.append(dict(file=path.name,source_sha256=hashlib.sha256(original).hexdigest(),output_sha256=hashlib.sha256(encoded).hexdigest(),changes=changed))
    (args.output/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
