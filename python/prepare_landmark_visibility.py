#!/usr/bin/env python3
"""Select inexpensive distant architecture across every level, without name lists."""
import argparse,hashlib,json,math,re,struct
from pathlib import Path
import numpy as np
from prepare_character_detail import nodes,triangles
from skin_inventory import pstring

def walk(tree):
    for node in tree:
        yield node
        yield from walk(node.children)
def fnv(data):
    value=2166136261
    for byte in data:value=((value^byte)*16777619)&0xffffffff
    return value

def candidates(path):
    tree=nodes(path.read_bytes());materials={}
    for node in walk(tree):
        if node.kind!=0x11000:continue
        name,off=pstring(node.body,0);kind,_=pstring(node.body,off+4)
        allowed=kind=='simple';texture=''
        for p in node.children:
            if p.kind==0x11003 and p.body[:4] in (b'BLMD',b'LIT\0'):
                allowed &= struct.unpack_from('<I',p.body,4)[0]==0
            if p.kind==0x11002 and p.body[:4]==b'TEX\0':texture=pstring(p.body,4)[0]
        allowed &= not bool(re.search(r'tree|leaf|palm|bush|grass|ocean|cloud|shadow|glow|anim_light|fire|smoke',texture,re.I))
        materials[name]=materials.get(name,True) and allowed
    result=[]
    for entity in walk(tree):
        if entity.kind!=0x3f00000:continue
        name,off=pstring(entity.body,0)
        if struct.unpack_from('<I',entity.body,off+4)[0]:continue
        mesh=next((n for n in entity.children if n.kind==0x10000),None)
        if mesh is None:continue
        groups=[n for n in mesh.children if n.kind==0x10002]
        if not 1<=len(groups)<=4:continue
        points=[];total_area=0.;up_area=0.;valid=True;vertices=0
        for group in groups:
            material,off=pstring(group.body,4);topology,fmt,count,index_count,bones=struct.unpack_from('<5I',group.body,off)
            if not materials.get(material) or bones or topology not in (0,1):valid=False;break
            parts={n.kind:n for n in group.children}
            if not {0x10005,0x1000a}<=parts.keys():valid=False;break
            p=np.frombuffer(parts[0x10005].body,dtype='<f4',offset=4).reshape((-1,3)).astype(np.float64)
            indices=struct.unpack_from(f'<{index_count}I',parts[0x1000a].body,4);faces=np.asarray(triangles(indices,topology),dtype=np.int32)
            if not len(faces):valid=False;break
            cross=np.cross(p[faces[:,1]]-p[faces[:,0]],p[faces[:,2]]-p[faces[:,0]]);area=np.linalg.norm(cross,axis=1)
            total_area+=float(area.sum());up_area+=float(area[np.abs(cross[:,1])>.6*area].sum())
            points.append(p);vertices+=count
        if not valid or vertices>2500 or total_area<1e-6 or up_area/total_area>.45:continue
        p=np.concatenate(points);low=p.min(axis=0);high=p.max(axis=0);extent=high-low
        horizontal=max(extent[0],extent[2]);height=extent[1]
        if height<12 or horizontal<12 or height/horizontal<.16:continue
        parts={n.kind:n for n in mesh.children}
        if not {0x10003,0x10004}<=parts.keys():continue
        box,sphere=parts[0x10003].body,parts[0x10004].body
        assert len(box)==24 and len(sphere)==16
        geometry_key=fnv(sphere+box)
        name_key=fnv(path.name.lower().encode()+b'\0'+name.encode())
        score=height*horizontal/(math.sqrt(vertices)*len(groups))
        result.append(dict(file=path.name,entity=name,name_key=name_key,geometry_key=geometry_key,vertices=vertices,draws=len(groups),height=float(height),width=float(horizontal),score=score,sphere=list(struct.unpack('<4f',sphere)),source_sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('art',type=Path);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--draw-budget',type=int,default=16);parser.add_argument('--vertex-budget',type=int,default=10000)
    parser.add_argument('--allocation',choices=('balanced-files','score'),default='balanced-files')
    args=parser.parse_args();assert 1<=args.draw_budget<=64 and 1<=args.vertex_budget<=100000
    args.output.mkdir(exist_ok=False);selected=[];levels=[]
    for level in '1234567':
        pool=[item for path in sorted(args.art.glob('*.p3d')) if re.fullmatch('l'+level+r'[rz].*\.p3d',path.name,re.I) for item in candidates(path)]
        draws=vertices=0;chosen=[]
        ranked=sorted(pool,key=lambda i:(-i['score'],i['file'],i['entity']))
        # Give each eligible streaming area a turn before selecting another piece
        # from the same area. Large industrial backgrounds must not consume the
        # entire city budget while downtown architecture receives no extension.
        if args.allocation=='balanced-files':
            ranks={};ordered=[]
            for item in ranked:
                rank=ranks.get(item['file'],0);ranks[item['file']]=rank+1
                ordered.append((rank,item))
            ranked=[item for _,item in sorted(ordered,key=lambda pair:(pair[0],-pair[1]['score'],pair[1]['file'],pair[1]['entity']))]
        for item in ranked:
            if draws+item['draws']>args.draw_budget or vertices+item['vertices']>args.vertex_budget:continue
            draws+=item['draws'];vertices+=item['vertices'];chosen.append(item)
        selected+=chosen;levels.append(dict(level=level,candidates=len(pool),candidate_files=len({i['file'] for i in pool}),selected=len(chosen),selected_files=len({i['file'] for i in chosen}),draws=draws,vertices=vertices,items=chosen))
    records=sorted((i['name_key'],i['geometry_key']) for i in selected);assert len(records)==len(set(records)) and len(records)<=448
    binary=struct.pack('<4sIII',b'LMV1',1,len(records),8)+b''.join(struct.pack('<II',*r) for r in records)
    (args.output/'landmark-visibility.bin').write_bytes(binary)
    (args.output/'manifest.json').write_text(json.dumps(dict(draw_budget=args.draw_budget,vertex_budget=args.vertex_budget,allocation=args.allocation,selection='opaque static architecture, geometric size/orientation and rendering cost; no hardcoded building names',levels=levels),indent=2)+'\n')
    print(json.dumps([{k:v for k,v in i.items() if k!='items'} for i in levels],indent=2))
if __name__=='__main__':main()
