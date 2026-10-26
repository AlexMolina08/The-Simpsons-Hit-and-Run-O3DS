#!/usr/bin/env python3
"""Bake restrained scenery contact shading into existing color bytes offline.

Output files keep their exact sizes, vertex/index counts, materials, alpha and
textures. Only RGB bytes in existing opaque world color lists can change. No
additional world geometry, texture, draw or runtime shading work is required.
"""
import argparse
import hashlib
import json
import math
import re
import struct
import subprocess
from pathlib import Path

from skin_inventory import chunks,pstring
from prepare_character_detail import triangles


def walk(data,start=0,end=None):
    for kind,body,child,stop in chunks(data,start,len(data) if end is None else end):
        yield kind,body,child,stop
        yield from walk(data,child,stop)


def collect(path):
    data=path.read_bytes();opaque={}
    for kind,body,child,stop in walk(data):
        if kind!=0x11000:continue
        material,_=pstring(data,body);allowed=True
        for k,b,c,e in chunks(data,child,stop):
            if k==0x11003 and data[b:b+4] in (b'BLMD',b'ATST'):
                if struct.unpack_from('<I',data,b+4)[0]:allowed=False
        opaque[material]=allowed
    positions=[];indices=[];patches=[];materials=[]
    skip=re.compile(r'butterfl|pidgeon|pigeon|girl|boy|head|skirt|cloud|horizon|sky|ocean|shadow',re.I)
    for kind,body,child,stop in walk(data):
        if kind!=0x10000:continue
        mesh,_=pstring(data,body)
        if skip.search(mesh):continue
        groups=[]
        for k,b,c,e in chunks(data,child,stop):
            if k!=0x10002:continue
            material,o=pstring(data,b+4);topology,fmt,count,index_count,bones=struct.unpack_from('<5I',data,o)
            if topology not in (0,1) or bones or not opaque.get(material,True):continue
            if re.search(r'glass|leaf|leav|water|fence|wire|trans|smoke|particle',material,re.I):continue
            parts={kk:(bb,cc) for kk,bb,cc,ee in chunks(data,c,e)}
            if not {0x10005,0x10008,0x1000A}<=parts.keys():continue
            p=parts[0x10005][0];colour=parts[0x10008][0]
            assert struct.unpack_from('<I',data,p)[0]==count
            assert struct.unpack_from('<I',data,colour)[0]==count
            points=list(struct.iter_unpack('<3f',data[p+4:p+4+12*count]))
            groups.append((material,points,colour,struct.unpack_from(f'<{index_count}I',data,parts[0x1000A][0]+4),topology))
        if not groups:continue
        points=[p for _,points,_,_,_ in groups for p in points]
        center=[(min(p[a] for p in points)+max(p[a] for p in points))*.5 for a in range(3)]
        # Local animated/instanced prototypes are not a world-space occluder.
        extent=max(max(p[a] for p in points)-min(p[a] for p in points) for a in range(3))
        if max(abs(x) for x in center)<50 and extent<20:continue
        for material,points,colour,source_indices,topology in groups:
            start=len(positions);positions.extend(points)
            indices.extend([[start+i for i in t] for t in triangles(source_indices,topology)])
            patches.append((start,len(points),colour));materials.append(material)
    return dict(path=path,data=data,positions=positions,indices=indices,patches=patches,materials=materials)


def sun_direction(level):
    direction=(.35,.82,-.45) if level not in '35' else (-.65,.42,-.63)
    size=math.sqrt(sum(x*x for x in direction))
    return tuple(x/size for x in direction)


def key_fill(normal,level,dark):
    """Restrained sun/moon direction, preserving authored color and dark detail."""
    night=level in '467'
    key=max(0.,sum(n*d for n,d in zip(normal,sun_direction(level))))
    up=max(0.,normal[1])
    shade=min(1.,(.90+.10*key+.04*up) if night else (.76+.25*key+.10*up))
    if dark:shade=max(.94,shade)
    # Keep authored palette; cool fill varies by less than two percent.
    return (shade*(.99 if night else 1.),shade, min(1.,shade*(1.01 if night else 1.015)))


def bake_level(sources,helper,radius,lighting=None,sun_shadows=False):
    positions=[];indices=[]
    for source in sources:
        source['start']=len(positions)
        indices.extend([[i+len(positions) for i in triangle] for triangle in source['indices']])
        positions.extend(source['positions'])
    if not positions or not indices:return []
    normals={p:[0.,0.,0.] for p in positions}
    for a,b,c in indices:
        pa,pb,pc=positions[a],positions[b],positions[c]
        u=[pb[i]-pa[i] for i in range(3)];v=[pc[i]-pa[i] for i in range(3)]
        n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
        for index in (a,b,c):
            target=normals[positions[index]]
            for axis in range(3):target[axis]+=n[axis]
    request=struct.pack('<IIIf',len(positions),len(indices),64,radius)
    vertices=[];unit_normals=[]
    for point in positions:
        normal=normals[point];size=math.sqrt(sum(x*x for x in normal))
        normal=[x/size for x in normal] if size>1e-10 else [0.,0.,0.]
        unit_normals.append(normal)
        vertices.append(struct.pack('<6f',*point,*normal))
    request+=b''.join(vertices)+b''.join(struct.pack('<3I',*t) for t in indices)
    shades=subprocess.run([str(helper.resolve())],input=request,capture_output=True,check=True).stdout
    assert len(shades)==len(positions)
    sunlight=None
    if sun_shadows:
        assert lighting in '1234567'
        sun_request=struct.pack('<IIIf',len(positions),len(indices),8,80.)+request[16:]
        sunlight=subprocess.run([str(helper.resolve()),'--sun',*(str(x) for x in sun_direction(lighting))],
                                input=sun_request,capture_output=True,check=True).stdout
        assert len(sunlight)==len(positions)
    result=[]
    for source in sources:
        data=source['data'];output=bytearray(data);changed=0;minimum=255
        for start,count,colour in source['patches']:
            for i in range(count):
                offset=colour+4+4*i;original=data[offset:offset+4]
                shade=255-round((255-shades[source['start']+start+i])*.85)
                # Preserve authored dark/night areas instead of crushing their detail.
                if max(original[:3])<80:shade=max(shade,235)
                minimum=min(minimum,shade)
                light=key_fill(unit_normals[source['start']+start+i],lighting,max(original[:3])<80) if lighting else (1.,1.,1.)
                direct=1.
                if sunlight is not None:
                    vertex=source['start']+start+i
                    facing=max(0.,sum(n*d for n,d in zip(unit_normals[vertex],sun_direction(lighting))))
                    strength=.06 if lighting in '467' else .34
                    direct=1-strength*facing*(1-sunlight[vertex]/255)
                for channel in range(3):
                    multiplier=shade/255*light[channel]*direct
                    if lighting:multiplier=max(.90 if max(original[:3])<80 else 177/255,multiplier)
                    output[offset+channel]=round(original[channel]*multiplier)
                assert output[offset+3]==original[3]
                changed+=output[offset:offset+3]!=original[:3]
        assert len(output)==len(data)
        result.append((source['path'],bytes(output),dict(vertices=len(source['positions']),
            triangles=len(source['indices']),changed=changed,minimum_multiplier=minimum/255,
            groups=len(source['patches']),materials=len(set(source['materials'])),
            context_files=len(sources),context_vertices=len(positions),radius=radius,key_fill_level=lighting,
            sun_cast_shadows=sun_shadows,sun_samples=8 if sun_shadows else 0,
            blocked_sun_vertices=sum(x<255 for x in sunlight[source['start']:source['start']+len(source['positions'])]) if sunlight is not None else 0)))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets',type=Path);parser.add_argument('--helper',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--levels',default='1234567')
    parser.add_argument('--radius',type=float,default=2.25)
    parser.add_argument('--key-fill',action='store_true',help='Bake restrained level-specific directional shape into existing RGB')
    parser.add_argument('--sun-shadows',action='store_true',help='Cast static sunlight/moonlight shadows across each level on the PC')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False);report=[]
    assert 0<args.radius<=5
    for level in args.levels:
        paths=[path for path in sorted(args.assets.iterdir())
               if re.fullmatch(r'l'+re.escape(level)+r'[rz]\d+[a-z]?\.p3d',path.name,re.I)]
        sources=[collect(path) for path in paths]
        for path,output,statistics in bake_level(sources,args.helper,args.radius,level if args.key_fill or args.sun_shadows else None,args.sun_shadows):
            if statistics['changed']:
                (args.output/path.name).write_bytes(output)
                entry=dict(file=path.name,bytes=len(output),source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                           output_sha256=hashlib.sha256(output).hexdigest(),**statistics)
                report.append(entry);print(json.dumps(entry),flush=True)
    (args.output/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()
