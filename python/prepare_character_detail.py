#!/usr/bin/env python3
"""Refine rigid pedestrian head edges offline; keep originals and animation bones.

Only unweighted high-detail skin groups participate. Original vertices, weights,
palettes, materials and textures remain intact. At most 64 new vertices per model
keep the existing GPU skin pools useful; no new CPU animation operation is added.
"""
import argparse
import hashlib
import json
import math
import struct
from collections import defaultdict
from pathlib import Path

from skin_inventory import chunks, pstring


class Node:
    def __init__(self, kind, body, children):
        self.kind, self.body, self.children = kind, body, children

    def encode(self):
        children=b''.join(child.encode() for child in self.children)
        header=12+len(self.body)
        return struct.pack('<III',self.kind,header,header+len(children))+self.body+children


def nodes(data,start=0,end=None):
    return [Node(kind,data[body:child],nodes(data,child,stop))
            for kind,body,child,stop in chunks(data,start,len(data) if end is None else end)]


def unit(v):
    size=math.sqrt(sum(x*x for x in v))
    return tuple(x/size for x in v) if size>1e-10 else (0.,1.,0.)


def triangles(indices,topology):
    result=[]
    for i in range(0,len(indices)-2,3 if topology==0 else 1):
        triangle=list(indices[i:i+3])
        if topology==1 and i%2:triangle[0],triangle[1]=triangle[1],triangle[0]
        if len(set(triangle))==3:result.append(triangle)
    return result


def group(node):
    if node.kind!=0x10002:return None
    material,offset=pstring(node.body,4)
    topology,fmt,count,index_count,bones=struct.unpack_from('<5I',node.body,offset)
    parts={n.kind:n for n in node.children}
    if topology not in (0,1) or fmt&256 or 0x1000C in parts:return None
    if not {0x10005,0x10006,0x10007,0x1000A,0x1000B,0x1000D}<=parts.keys():return None
    allowed={0x10005,0x10006,0x10007,0x10008,0x1000A,0x1000B,0x1000D,0x10010,0x10011}
    if set(parts)-allowed:return None
    position=list(struct.iter_unpack('<3f',parts[0x10005].body[4:]))
    normal=list(struct.iter_unpack('<3f',parts[0x10006].body[4:]))
    uv=list(struct.iter_unpack('<2f',parts[0x10007].body[8:]))
    matrix=[parts[0x1000B].body[4+4*i:8+4*i] for i in range(count)]
    palette=struct.unpack_from(f'<{bones}I',parts[0x1000D].body,4)
    colour=list(struct.iter_unpack('<4B',parts[0x10008].body[4:])) if 0x10008 in parts else None
    indices=struct.unpack_from(f'<{index_count}I',parts[0x1000A].body,4)
    assert len(position)==len(normal)==len(uv)==len(matrix)==count
    return dict(node=node,parts=parts,offset=offset,fmt=fmt,bones=bones,material=material,
                position=position,normal=normal,uv=uv,matrix=matrix,palette=palette,colour=colour,
                triangles=triangles(indices,topology),original=count)


def refine_skin(skin,budget):
    groups=[g for child in skin.children if (g:=group(child))]
    if not groups:return 0
    positions=[p for g in groups for p in g['position']]
    low=[min(p[a] for p in positions) for a in range(3)]
    high=[max(p[a] for p in positions) for a in range(3)]
    sphere=next((struct.unpack('<4f',n.body) for n in skin.children if n.kind==0x10004),None)
    if sphere:
        sphere_limit=max(sphere[3],max(math.sqrt(sum((p[a]-sphere[a])**2 for a in range(3))) for p in positions))
    threshold=low[1]+.68*(high[1]-low[1])
    normals=defaultdict(list)

    def key(g,i):return g['position'][i]+(g['palette'][g['matrix'][i][3]],)
    for g in groups:
        for i,n in enumerate(g['normal']):normals[key(g,i)].append(n)
    averaged={k:unit([sum(n[a] for n in values) for a in range(3)]) for k,values in normals.items()}
    candidates=defaultdict(list)
    for gi,g in enumerate(groups):
        for triangle in g['triangles']:
            for a,b in zip(triangle,triangle[1:]+triangle[:1]):
                if min(g['position'][a][1],g['position'][b][1])<threshold:continue
                if g['matrix'][a][3]!=g['matrix'][b][3]:continue
                ka,kb=key(g,a),key(g,b)
                edge=tuple(sorted((ka,kb)))
                item=(gi,min(a,b),max(a,b))
                if item not in candidates[edge]:candidates[edge].append(item)
    ranked=[]
    for edge,copies in candidates.items():
        a,b=edge;na,nb=averaged[a],averaged[b]
        curvature=max(0.,1-sum(x*y for x,y in zip(na,nb)))
        length=math.sqrt(sum((a[i]-b[i])**2 for i in range(3)))
        if curvature>.002 and .002<length<.5:ranked.append((curvature*length,edge,copies))
    chosen=[];used=0
    for _,edge,copies in sorted(ranked,reverse=True):
        if used+len(copies)>budget:continue
        chosen.append((edge,copies));used+=len(copies)
    split=[{} for _ in groups]
    vertex_edges={};linear={}
    for edge,copies in chosen:
        a,b=edge;mid=tuple((a[i]+b[i])*.5 for i in range(3))
        linear[edge]=struct.unpack('<3f',struct.pack('<3f',*mid))
        na,nb=averaged[a],averaged[b]
        projected=[]
        for p,n in ((a,na),(b,nb)):
            distance=sum((mid[i]-p[i])*n[i] for i in range(3))
            projected.append([mid[i]-distance*n[i] for i in range(3)])
        curved=[(projected[0][i]+projected[1][i])*.5 for i in range(3)]
        delta=[(curved[i]-mid[i])*.8 for i in range(3)]
        distance=math.sqrt(sum(x*x for x in delta))
        scale=min(1.,.02/distance) if distance else 1.
        curved=struct.unpack('<3f',struct.pack('<3f',*(max(low[i],min(high[i],mid[i]+delta[i]*scale)) for i in range(3))))
        if sphere and sum((curved[i]-sphere[i])**2 for i in range(3))>sphere_limit**2:
            # Preserve the authored culling sphere as well as the box. Search
            # along the original midpoint-to-curve segment, shared by all seams.
            low_t,high_t=0.,1.
            for _ in range(32):
                t=(low_t+high_t)*.5
                point=tuple(mid[i]+(curved[i]-mid[i])*t for i in range(3))
                if sum((point[i]-sphere[i])**2 for i in range(3))<=sphere_limit**2:low_t=t
                else:high_t=t
            curved=struct.unpack('<3f',struct.pack('<3f',*(mid[i]+(curved[i]-mid[i])*low_t for i in range(3))))
        for gi,a,b in copies:
            g=groups[gi];index=len(g['position']);split[gi][(a,b)]=index
            vertex_edges[gi,index]=edge
            g['position'].append(curved);g['normal'].append(unit([g['normal'][a][i]+g['normal'][b][i] for i in range(3)]))
            g['uv'].append(tuple((g['uv'][a][i]+g['uv'][b][i])*.5 for i in range(2)))
            g['matrix'].append(g['matrix'][a])
            if g['colour'] is not None:g['colour'].append(tuple(round((g['colour'][a][i]+g['colour'][b][i])*.5) for i in range(4)))
    outputs={};parents={}
    for gi,g in enumerate(groups):
        if not split[gi]:continue
        output=[]
        def middle(a,b):return split[gi].get(tuple(sorted((a,b))))
        for a,b,c in g['triangles']:
            mids=[middle(a,b),middle(b,c),middle(c,a)];count=sum(m is not None for m in mids)
            if count==0:output.append((a,b,c));parents[gi,len(output)-1]=(a,b,c);continue
            first=len(output);original=(a,b,c)
            while (count==1 and mids[0] is None) or (count==2 and (mids[0] is None or mids[1] is None)):
                a,b,c=b,c,a;mids=mids[1:]+mids[:1]
            m,n,p=mids
            if count==1:output.extend(((a,m,c),(m,b,c)))
            elif count==2:output.extend(((m,b,n),(a,m,n),(a,n,c)))
            else:output.extend(((a,m,p),(m,b,n),(p,n,c),(m,n,p)))
            for i in range(first,len(output)):parents[gi,i]=original
        outputs[gi]=output
    # Thin source triangles can reverse after a curved edge moves out of their
    # plane. Fall back to linear midpoints for every shared copy of a bad edge.
    # Validate float32 positions before serialization, including material seams.
    def cross(g,face):
        a,b,c=[g['position'][i] for i in face]
        u,v=[b[i]-a[i] for i in range(3)],[c[i]-a[i] for i in range(3)]
        return (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
    for attempt in range(budget+1):
        rejected=set()
        for gi,output in outputs.items():
            g=groups[gi]
            for i,face in enumerate(output):
                if sum(a*b for a,b in zip(cross(g,face),cross(g,parents[gi,i]))) < -1e-12:
                    rejected.update(vertex_edges[gi,v] for v in face if (gi,v) in vertex_edges)
        if not rejected:break
        changed=False
        for (gi,v),edge in vertex_edges.items():
            if edge in rejected and groups[gi]['position'][v]!=linear[edge]:
                groups[gi]['position'][v]=linear[edge];changed=True
        if not changed:raise ValueError('Unstable source triangle; preserve this model without refinement')
    else:raise ValueError('Head refinement did not converge')
    for gi,output in outputs.items():
        g=groups[gi]
        count=len(g['position']);parts=g['parts']
        for kind,values,fmt in ((0x10005,g['position'],'3f'),(0x10006,g['normal'],'3f'),(0x1000B,g['matrix'],None)):
            body=b''.join(struct.pack('<'+fmt,*v) for v in values) if fmt else b''.join(values)
            parts[kind].body=struct.pack('<I',count)+body
        parts[0x10007].body=struct.pack('<I',count)+parts[0x10007].body[4:8]+b''.join(struct.pack('<2f',*v) for v in g['uv'])
        if g['colour'] is not None:parts[0x10008].body=struct.pack('<I',count)+b''.join(bytes(v) for v in g['colour'])
        parts[0x1000A].body=struct.pack('<I',3*len(output))+b''.join(struct.pack('<3I',*v) for v in output)
        body=bytearray(g['node'].body);struct.pack_into('<5I',body,g['offset'],0,g['fmt'],count,3*len(output),g['bones']);g['node'].body=bytes(body)
        g['node'].children=[n for n in g['node'].children if n.kind!=0x10010]
    return used


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets',type=Path);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--budget',type=int,default=64)
    args=parser.parse_args();assert 0<args.budget<=128
    names={'boy1','boy2','boy3','boy4','girl1','girl2','girl3','girl4','fem1','fem2','fem3','fem4',
           'male1','male2','male3','male4','male5','male6','busm1','busm2','busw1','bum','hooker','mobstr','nuclear',
           'const1','const2','joger1','joger2','olady1','olady2','olady3','rednk1','rednk2','farmr1',
           'sail1','sail2','sail3','sail4','zfem1','zfem5','zmale1','zmale3'}
    args.output.mkdir(parents=True,exist_ok=False);report=[]
    for path in sorted(args.assets.glob('*_m.p3d')):
        data=path.read_bytes();tree=nodes(data);changed=0
        def walk(items):
            nonlocal changed
            for item in items:
                if item.kind==0x10001:
                    name,_=pstring(item.body,0)
                    if name.removesuffix('_h_merged') in names:changed+=refine_skin(item,args.budget)
                else:walk(item.children)
        walk(tree)
        if changed:
            output=b''.join(item.encode() for item in tree);(args.output/path.name).write_bytes(output)
            report.append(dict(file=path.name,new_vertices=changed,source_sha256=hashlib.sha256(data).hexdigest(),
                               output_sha256=hashlib.sha256(output).hexdigest(),source_bytes=len(data),output_bytes=len(output)))
    (args.output/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(models=len(report),new_vertices=sum(x['new_vertices'] for x in report),maximum_per_model=max(x['new_vertices'] for x in report))))


if __name__=='__main__':main()
