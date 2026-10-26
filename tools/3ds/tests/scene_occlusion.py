#!/usr/bin/env python3
"""Prove that separate scenery files share occluders without altering alpha."""
import argparse
from pathlib import Path
import struct
import sys
import subprocess

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from prepare_scene_occlusion import bake_level,key_fill

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--helper',required=True,type=Path)
args=parser.parse_args()
ground=dict(path=Path('ground.p3d'),data=struct.pack('<I',3)+bytes([200,200,200,173])*3,
    positions=[(0,0,0),(0,0,2),(2,0,0)],indices=[[0,1,2]],patches=[(0,3,0)],materials=['ground'])
roof=dict(path=Path('roof.p3d'),data=b'',positions=[(-3,1,-3),(3,1,-3),(3,1,3),(-3,1,3)],
    indices=[[0,1,2],[0,2,3]],patches=[],materials=[])
clear=bake_level([dict(ground)],args.helper,2.25)[0][1]
shaded=bake_level([dict(ground),dict(roof)],args.helper,2.25)[0][1]
assert shaded[4]<clear[4]
assert shaded[:4]==clear[:4] and shaded[7::4]==clear[7::4]
far=dict(roof,positions=[(x,y+10,z) for x,y,z in roof['positions']])
outside=bake_level([dict(ground),far],args.helper,2.25)[0][1]
assert outside==clear, 'geometry outside the radius should not cast this contact shade'
dark=dict(ground,data=struct.pack('<I',3)+bytes([40,60,20,173])*3)
night=bake_level([dark,dict(roof)],args.helper,2.25)[0][1]
for i in range(3):
    assert night[4+4*i]>=36 and night[5+4*i]>=55 and night[6+4*i]>=18
assert night[7::4]==dark['data'][7::4]
for level in '1234567':
    for normal in ((1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1),(0,0,0)):
        for dark_area in (False,True):
            light=key_fill(normal,level,dark_area)
            assert all((.9306 if dark_area else .75)<=channel<=1 for channel in light)
    lit=bake_level([dict(ground)],args.helper,2.25,level)[0][1]
    assert len(lit)==len(clear) and lit[:4]==clear[:4] and lit[7::4]==clear[7::4]
assert key_fill((1,0,0),'1',False)[0]>key_fill((-1,0,0),'1',False)[0]
assert min(key_fill((-1,0,0),'4',False))>min(key_fill((-1,0,0),'1',False))
sun_clear=bake_level([dict(ground)],args.helper,2.25,'1',True)[0][1]
sun_shaded=bake_level([dict(ground),dict(roof)],args.helper,2.25,'1',True)[0][1]
assert sun_shaded[4]<sun_clear[4]
assert sun_shaded[7::4]==ground['data'][7::4] and len(sun_shaded)==len(sun_clear)
sun_far=dict(roof,positions=[(x,y+100,z) for x,y,z in roof['positions']])
assert bake_level([dict(ground),sun_far],args.helper,2.25,'1',True)[0][1]==sun_clear
for level in '1234567':
    shadowed=bake_level([dict(dark),dict(roof)],args.helper,2.25,level,True)[0][1]
    for i in range(3):
        for channel in range(3):
            before=dark['data'][4+4*i+channel];after=shadowed[4+4*i+channel]
            assert before*.90-1<=after<=before
for height,expected in ((1.,0),(-1.,255),(100.,255)):
    vertices=[(0,0,0,0,1,0),(-3,height,-3,0,1,0),(3,height,-3,0,1,0),
              (3,height,3,0,1,0),(-3,height,3,0,1,0)]
    request=struct.pack('<IIIf',5,2,8,80.)+b''.join(struct.pack('<6f',*v) for v in vertices)+struct.pack('<6I',1,2,3,1,3,4)
    result=subprocess.run([str(args.helper.resolve()),'--sun','0','1','0'],input=request,capture_output=True,check=True).stdout
    assert len(result)==5 and result[0]==expected,(height,result)
assert subprocess.run([str(args.helper.resolve()),'--sun','0','0','0'],input=b'',capture_output=True).returncode==7
print('PASS: another file casts contact shading, radius excludes distant geometry, '
      'finite sun visibility and 80m range, authored dark colors stay readable, '
      'seven-level directional bounds, alpha and sizes unchanged. Not hardware proof.')
