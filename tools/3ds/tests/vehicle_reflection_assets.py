#!/usr/bin/env python3
"""Prove windshield copies preserve geometry, textures, alpha and other materials."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from prepare_character_detail import nodes
from skin_inventory import pstring

def check(source,output):
    changes=0
    def compare(a,b):
        nonlocal changes
        assert len(a)==len(b)
        for x,y in zip(a,b):
            assert x.kind==y.kind
            if x.kind!=0x11000 or x.body==y.body:
                assert x.body==y.body;compare(x.children,y.children);continue
            name,off=pstring(x.body,0);newname,new_off=pstring(y.body,0)
            kind,tail=pstring(x.body,off+4);newkind,new_tail=pstring(y.body,new_off+4)
            assert name==newname and kind=='simple' and newkind=='vehicle_glass'
            assert x.body[off:off+4]==y.body[new_off:new_off+4]
            state=list(struct.unpack_from('<4I',x.body,tail));state[-1]+=2
            assert y.body[new_tail:]==struct.pack('<4I',*state)
            assert len(y.children)==len(x.children)+2
            assert [n.encode() for n in x.children]==[n.encode() for n in y.children[:-2]]
            assert y.children[-2].kind==0x11002 and y.children[-2].body[:4]==b'REFL'
            assert y.children[-1].kind==0x11005 and y.children[-1].body==b'ENVB'+bytes([48,48,48,255])
            changes+=1
    compare(nodes(source.read_bytes()),nodes(output.read_bytes()))
    return changes

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cars',type=Path);parser.add_argument('derived',type=Path);args=parser.parse_args()
    manifest=json.loads((args.derived/'manifest.json').read_text());changes=0
    for item in manifest:
        source=args.cars/item['file'];assert hashlib.sha256(source.read_bytes()).hexdigest()==item['source_sha256']
        changes+=check(source,args.derived/item['file'])
    print(f'PASS: {len(manifest)} cars, {changes} windshield materials. Exact original geometry, textures, blend modes, alpha, body reflection strength and other shader parameters.')
if __name__=='__main__':main()
