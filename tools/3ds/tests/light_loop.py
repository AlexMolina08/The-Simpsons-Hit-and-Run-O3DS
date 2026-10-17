#!/usr/bin/env python3
"""Execute production lighting bodies with rounded float24 scalar operations."""
from pathlib import Path
import math
import random
import re
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
shaders=ROOT/'libs/pure3d/pddi/pica/shaders'
with tempfile.TemporaryDirectory(prefix='srr-lights-') as td:
 for base in ['pddi','skin']:
  original=(shaders/(base+'.v.pica')).read_text()
  optimized=(shaders/(base+'_lights.v.pica')).read_text()
  expected=original.replace('.consti i_lights(3, 0, 1, 0)', '.ivec u_lightLoop\n.bool u_hasLights').replace('\tfor i_lights','\tifu u_hasLights\n\tfor u_lightLoop').replace('\tmax\tr0, zeros, r0','\t.end\n\n\tmax\tr0, zeros, r0')
  assert optimized==expected,'Specialized shader drifted from original arithmetic'
  h=Path(td)/(base+'.h');b=Path(td)/(base+'.shbin')
  subprocess.run(['/opt/devkitpro/tools/bin/picasso','-o',str(b),'-h',str(h),str(shaders/(base+'_lights.v.pica'))],check=True)
  header=h.read_text();assert '#define VSH_IVEC_u_lightLoop 0x00' in header and '#define VSH_FLAG_u_hasLights BIT(0)' in header
  if base=='skin':assert '#define VSH_FVEC_u_bones 0x1B' in header and '#define VSH_ULEN_u_bones 69' in header
  binary=b.read_bytes();off=binary.index(b'DVLP');instructions=struct.unpack_from('<I',binary,off+12)[0]
  assert instructions<=512
  print(f'{base}: {instructions} instructions; light controls and uniform locations verified')
context=(ROOT/'libs/pure3d/pddi/pica/context.cpp').read_text()
assert '0x60 + VSH_IVEC_u_lightLoop' in context and 'nActiveLights ? nActiveLights - 1 : 0, 0, 1, 0' in context
assert 'C3D_BoolUnifSet(GPU_VERTEX_SHADER, 0x68, nActiveLights != 0)' in context
code=(shaders/'pddi.v.pica').read_text().split('.proc DoLighting\n')[1].splitlines()
# Expand only FOR/IF control. Every arithmetic instruction comes from the actual
# production shader and runs unchanged in both variants.
start=next(i for i,s in enumerate(code) if s.strip().startswith('for '))
end=next(i for i in range(start+1,len(code)) if code[i].strip()=='.end')
def f24(x):
 if not math.isfinite(x) or not x:return x
 m,e=math.frexp(x);return math.ldexp(round(m*131072)/131072,e)
def run(uniforms,active):
 regs={f'r{i}':[0.0]*4 for i in range(16)};regs.update(uniforms);aL=0
 def read(arg):
  negative=arg.startswith('-');arg=arg.lstrip('-')
  if arg=='zeros':return [0.0]*4
  if arg=='ones':return [1.0]*4
  name,_,swizzle=arg.partition('.');swizzle=swizzle.translate(str.maketrans('rgba','xyzw'))
  if '[aL]' in name:value=regs[name.replace('[aL]','')][aL]
  else:value=regs[name]
  swizzle=(swizzle or 'xyzw');swizzle=(swizzle+swizzle[-1]*4)[:4]
  return [(-1 if negative else 1)*value['xyzw'.index(c)] for c in swizzle]
 def execute(lines):
  for line in lines:
   line=line.strip()
   if not line or line=='.end':continue
   op,args=line.split(None,1);args=[a.strip() for a in args.split(',')]
   a=read(args[1]);b=read(args[2]) if len(args)>2 else None
   if op=='mov':value=a
   elif op in ('dp3','dp4'):value=[f24(sum(f24(a[i]*b[i]) for i in range(int(op[-1]))))]*4
   elif op=='rsq':value=[1/math.sqrt(a[0])]*4
   elif op=='rcp':value=[1/a[0]]*4
   elif op=='mad':
    c=read(args[3]);value=[a[i]*b[i]+c[i] for i in range(4)]
   else:
    f={'mul':lambda a,b:a*b,'add':lambda a,b:a+b,'max':max,'min':min}[op];value=[f(a[i],b[i]) for i in range(4)]
   name,_,mask=args[0].partition('.');mask=mask.translate(str.maketrans('rgba','xyzw'))
   for c in mask or 'xyzw':
    index='xyzw'.index(c);regs[name][index]=f24(value[index])
 execute(code[:start])
 for aL in range(active):execute(code[start+1:end])
 execute(code[end+1:]);return regs['r0']
rng=random.Random(93700)
for case in range(6000):
 active=case%5
 uniforms={'in_normal':[.2,.7,.5,0],'r8':[rng.uniform(-50,50) for i in range(3)]+[1],
 'u_mv':None,'u_lightPos':[],'u_lightColour':[],'u_lightAtten':[]}
 # Rows use the same names as the assembly's constant operands.
 for row in range(3):uniforms[f'u_mv[{row}]']=[1.0 if row==i else 0.0 for i in range(4)]
 for name in ['u_matEmissive','u_sceneAmbient','u_matAmbient','u_matDiffuse']:uniforms[name]=[f24(rng.random()) for i in range(4)]
 for i in range(4):
  if i<active:
   directional=(case+i)%2==0
   uniforms['u_lightPos'].append([f24(rng.uniform(.2,1)) for c in range(3)]+[0 if directional else 1])
   uniforms['u_lightColour'].append([f24(rng.random()) for c in range(3)]+[1])
   uniforms['u_lightAtten'].append([1,0,0,0] if directional else [.9,.1,.01,0])
  else:
   uniforms['u_lightPos'].append([0,0,1,0]);uniforms['u_lightColour'].append([0,0,0,0]);uniforms['u_lightAtten'].append([1,0,0,0])
 assert run(uniforms,4)==run(uniforms,active)
print('6000 production lighting cases: exact float24 color equality, 0..4 enabled lights, directional/point mixtures, ambient/emissive and alpha.')
