#!/usr/bin/env python3
"""Compare production original/reduced shader transforms with float24 arithmetic."""
from pathlib import Path
import math,random,re,struct,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
original=(root/'libs/pure3d/pddi/pica/shaders/pddi_lights.v.pica').read_text()
optimized=(root/'libs/pure3d/pddi/pica/shaders/pddi_unlit_projection.v.pica').read_text()
assert optimized[optimized.index('\tmov\to_pos'):]==original[original.index('\tmov\to_pos'):]
a=optimized.index('\t.else',optimized.index('\tifu u_fastUnlit'))+len('\t.else\n');b=optimized.index('\t.end',a)
assert optimized[a:b]==original[original.index('\tdp4\tr8.x'):original.index('\tmov\to_pos')]
code=optimized.split('.proc vmain\n')[1].split('\tmov\to_pos')[0]
oldcode=original.split('.proc vmain\n')[1].split('\tmov\to_pos')[0]
with tempfile.TemporaryDirectory(prefix='srr-unlit-') as td:
 p=Path(td)
 subprocess.run(['/opt/devkitpro/tools/bin/picasso','-o',str(p/'shader.shbin'),str(root/'libs/pure3d/pddi/pica/shaders/pddi_unlit_projection.v.pica')],check=True)
 cpp=r'''
#include <pddi/pica/unlitprojection.hpp>
#include <cassert>
#include <limits>
int main(){float p[4][4]={{0,1,0,0},{2,0,0,0},{0,0,3,4},{0,0,5,0}},s[4];
 assert(picaUnlitProjection::Parts(p,s)&&s[0]==1&&s[1]==2&&s[2]==3&&s[3]==5);
 for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){
  float old=p[i][j];p[i][j]=std::numeric_limits<float>::infinity();assert(!picaUnlitProjection::Parts(p,s));
  p[i][j]=std::numeric_limits<float>::quiet_NaN();assert(!picaUnlitProjection::Parts(p,s));p[i][j]=old;
 }
 unsigned zeroes[][2]={{0,0},{0,2},{0,3},{1,1},{1,2},{1,3},{2,0},{2,1},{3,0},{3,1},{3,3}};
 for(auto& pair:zeroes){p[pair[0]][pair[1]]=.1f;assert(!picaUnlitProjection::Parts(p,s));p[pair[0]][pair[1]]=0;}
}
'''
 (p/'test.cpp').write_text(cpp);subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
def f24(x,truncate=False):
 if not x:return x
 m,e=math.frexp(x);return math.ldexp((math.trunc(m*131072) if truncate else round(m*131072))/131072,e)
def execute(code,regs,fast,truncate):
 active=True
 def read(x):
  if x.startswith('-'):return [-v for v in read(x[1:])]
  name,_,swizzle=x.partition('.');value=regs[name]
  if not swizzle:return value[:]
  swizzle=swizzle*4 if len(swizzle)==1 else swizzle
  return [value['xyzw'.index(c)] for c in swizzle]
 instructions=0
 for line in code.strip().splitlines():
  line=line.strip()
  if not line:continue
  if line.startswith('ifu '):active=fast;continue
  if line=='.else':active=not active;continue
  if line=='.end':active=True;continue
  if not active:continue
  op,args=line.split(None,1);dest,*src=[s.strip() for s in args.split(',')];v=[read(s) for s in src];instructions+=1
  if op=='mov':result=v[0]
  elif op=='dp4':
   value=0
   for x,y in zip(*v):value=f24(value+f24(x*y,truncate),truncate)
   result=[value]*4
  elif op=='mul':result=[f24(x*y,truncate) for x,y in zip(*v)]
  elif op=='add':result=[f24(x+y,truncate) for x,y in zip(*v)]
  else:raise AssertionError(op)
  name,_,mask=dest.partition('.');mask=mask or 'xyzw'
  for c in mask:regs[name]['xyzw'.index(c)]=result['xyzw'.index(c)]
 return regs['r1'],instructions
rng=random.Random(19021)
for truncate in [False,True]:
 for case in range(20000):
  q=lambda v:f24(v,truncate)
  view=[[q(rng.uniform(-1,1)) for _ in range(3)]+[q(rng.uniform(-2048,2048))] for _ in range(3)]+[[0,0,0,1]]
  projection=[[0,q(rng.uniform(-3,-.2)),0,0],[q(rng.uniform(.2,3)),0,0,0],[0,0,q(rng.uniform(-2,-.5)),q(rng.uniform(-20,-.1))],[0,0,-1,0]]
  regs={f'r{i}':[0.0]*4 for i in range(16)}
  regs.update({'in_pos':[q(rng.uniform(-2048,2048)) for _ in range(3)]+[1], 'ones':[1]*4,
   'u_eye':[0,7,0,0],'u_stereoOn':[case%2,0,0,0],'u_unlitScale':[projection[0][1],projection[1][0],projection[2][2],projection[3][2]]})
  for i in range(4):regs[f'u_mv[{i}]']=view[i];regs[f'u_proj[{i}]']=projection[i]
  original_result,old_count=execute(oldcode,{k:v[:] for k,v in regs.items()},False,truncate)
  actual,new_count=execute(code,{k:v[:] for k,v in regs.items()},True,truncate)
  assert actual==original_result,(case,truncate,actual,original_result)
print(f'PASS: 40,000 exact finite clip-position comparisons, float24 rounding/truncation, original lit/stereo path retained; transform instructions {old_count} to {new_count}')
