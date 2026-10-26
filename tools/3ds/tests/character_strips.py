#!/usr/bin/env python3
"""Check strip winding, degenerate joins and exact prepared-character attributes."""
import argparse
from collections import Counter
import json
from pathlib import Path
import random
import struct
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from prepare_character_detail import nodes,triangles
from skin_inventory import pstring
from strip_character_detail import stripify,oriented

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--before',type=Path,required=True);parser.add_argument('--after',type=Path,required=True)
args=parser.parse_args();rng=random.Random(251026)
for trial in range(100):
    size=rng.randrange(2,12);faces=[]
    for y in range(size-1):
        for x in range(size-1):
            a=y*size+x;b=a+1;c=a+size;d=c+1
            faces.extend([(a,b,c),(c,b,d)])
    if trial%3==0:faces=[face for face in faces if rng.random()>.2]
    if trial%5==0:faces=[tuple(reversed(f)) if rng.random()>.5 else f for f in faces]
    if trial%7==0:faces+=faces[:3]
    rng.shuffle(faces)
    output=stripify(faces)
    assert Counter(map(oriented,triangles(output,1)))==Counter(map(oriented,faces))
assert stripify([])==[]

changed=0;indices_saved=0
def compare(before,after):
    global changed,indices_saved
    assert len(before)==len(after)
    for a,b in zip(before,after):
        assert a.kind==b.kind
        if a.kind==0x10002 and a.body!=b.body:
            material,offset=pstring(a.body,4);old=list(struct.unpack_from('<5I',a.body,offset));new=list(struct.unpack_from('<5I',b.body,offset))
            assert old[0]==0 and new[0]==1 and old[1:3]==new[1:3] and old[4]==new[4]
            assert a.body[:offset]==b.body[:offset] and a.body[offset+20:]==b.body[offset+20:]
            aa={n.kind:n for n in a.children};bb={n.kind:n for n in b.children};assert aa.keys()==bb.keys()
            for kind in aa:
                if kind!=0x1000A:assert aa[kind].encode()==bb[kind].encode()
            old_indices=struct.unpack_from(f'<{old[3]}I',aa[0x1000A].body,4)
            new_indices=struct.unpack_from(f'<{new[3]}I',bb[0x1000A].body,4)
            assert Counter(map(oriented,triangles(old_indices,0)))==Counter(map(oriented,triangles(new_indices,1)))
            assert max(new_indices)<new[2] and new[3]<old[3]
            changed+=1;indices_saved+=old[3]-new[3]
        else:
            assert a.body==b.body
            compare(a.children,b.children)
for record in json.loads((args.after/'manifest.json').read_text()):
    compare(nodes((args.before/record['file']).read_bytes()),nodes((args.after/record['file']).read_bytes()))
assert changed>0
print(f'PASS: 100 strip meshes, winding/joins, {changed} real groups; {indices_saved} indices removed; all vertex attributes, triangles and unrelated chunks identical. Not hardware timing.')
