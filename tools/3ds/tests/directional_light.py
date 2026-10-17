#!/usr/bin/env python3
"""Check prepared directional lights against production float24 lighting bodies."""
from pathlib import Path
import math,random,re,struct,subprocess,tempfile,copy
ROOT=Path(__file__).resolve().parents[3]
SHADERS=ROOT/'libs/pure3d/pddi/pica/shaders'

def f24(x):
 if not math.isfinite(x) or not x:return x
 m,e=math.frexp(x);return math.ldexp(round(m*131072)/131072,e)

def run(source,uniforms,active,prepared):
 code=source.split('.proc DoLighting\n',1)[1].splitlines()
 regs={f'r{i}':[0.0]*4 for i in range(16)};regs.update(copy.deepcopy(uniforms));aL=0;operations=0
 def read(arg):
  negative=arg.startswith('-');arg=arg.lstrip('-')
  if arg=='zeros':return [0.0]*4
  if arg=='ones':return [1.0]*4
  name,_,swizzle=arg.partition('.');swizzle=swizzle.translate(str.maketrans('rgba','xyzw'))
  value=regs[name.replace('[aL]','')][aL] if '[aL]' in name else regs[name]
  swizzle=swizzle or 'xyzw';swizzle=(swizzle+swizzle[-1]*4)[:4]
  return [(-1 if negative else 1)*value['xyzw'.index(c)] for c in swizzle]
 def execute(lines):
  nonlocal aL,operations
  i=0
  while i<len(lines):
   line=lines[i].strip();i+=1
   if not line or line=='.end':continue
   if line.startswith(('ifu ','for ')):
    start=i;depth=1;otherwise=None
    while depth:
     text=lines[i].strip()
     if text.startswith(('ifu ','for ')):depth+=1
     elif text=='.end':depth-=1
     elif text=='.else' and depth==1:otherwise=i
     i+=1
    stop=i-1
    if line.startswith('for '):
     for aL in range(active):execute(lines[start:stop])
    else:
     condition=active>0 if line=='ifu u_hasLights' else prepared
     if condition:execute(lines[start:otherwise if otherwise is not None else stop])
     elif otherwise is not None:execute(lines[otherwise+1:stop])
    continue
   op,args=line.split(None,1);args=[a.strip() for a in args.split(',')];operations+=1
   a=read(args[1]);b=read(args[2]) if len(args)>2 else None
   if op=='mov':value=a
   elif op in ('dp3','dp4'):value=[f24(sum(f24(a[j]*b[j]) for j in range(int(op[-1]))))]*4
   elif op=='rsq':value=[1/math.sqrt(a[0])]*4
   elif op=='rcp':value=[1/a[0]]*4
   elif op=='mad':
    c=read(args[3]);value=[a[j]*b[j]+c[j] for j in range(4)]
   else:
    fn={'mul':lambda a,b:a*b,'add':lambda a,b:a+b,'max':max,'min':min}[op]
    value=[fn(a[j],b[j]) for j in range(4)]
   name,_,mask=args[0].partition('.');mask=mask.translate(str.maketrans('rgba','xyzw'))
   for c in mask or 'xyzw':j='xyzw'.index(c);regs[name][j]=f24(value[j])
 execute(code);return regs['r0'],operations

with tempfile.TemporaryDirectory(prefix='srr-directional-light-') as td:
 td=Path(td)
 cpp=r'''
#include <pddi/pica/directionallight.hpp>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <limits>
#include <cstdint>
struct Vector{float w,z,y,x;};
static uint32_t state=846239;static unsigned Next(){state^=state<<13;state^=state>>17;return state^=state<<5;}
int main(int argc,char**argv){
 assert(argc==2);std::ofstream output(argv[1],std::ios::binary);
 for(unsigned test=0;test<12000;++test){
  Vector v={0,0,0,0},p={};float scale=test%3==0?.001f:test%3==1?1.f:1000.f;
  v.x=(int(Next()%20001)-10000)*scale;v.y=(int(Next()%20001)-10000)*scale;v.z=(int(Next()%20001)-10000)*scale;
  if(test%3==2){v.x*=.01f;v.y*=.01f;v.z*=.01f;}
  const float len=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
  v.x/=len;v.y/=len;v.z/=len;
  assert(picaDirectionalLight::Prepare(v,p));
  assert(std::fabs((p.x*p.x+p.y*p.y)+p.z*p.z-1.f)<.000001f);
  float data[]={v.x,v.y,v.z,p.x,p.y,p.z};output.write(reinterpret_cast<const char*>(data),sizeof(data));
 }
 Vector v={0,1,1,1},p={};
 const float bad[]={0,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),1.e-10f,1.e10f};
 for(float x:bad){v.x=v.y=v.z=x;assert(!picaDirectionalLight::Prepare(v,p));}
 v={0,2,0,0};assert(!picaDirectionalLight::Prepare(v,p));
 v={1,1,1,1};assert(!picaDirectionalLight::Prepare(v,p));
 v={-0.f,0,0,-1};assert(picaDirectionalLight::Prepare(v,p));assert(p.x==-1&&p.w==0);
 puts("PASS: 12000 production preparations; finite ranges, degenerate/point fallbacks and reversed component storage");
}
'''
 (td/'test.cpp').write_text(cpp)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(ROOT/'libs/pure3d'),str(td/'test.cpp'),'-o',str(td/'test')],check=True)
 subprocess.run([str(td/'test'),str(td/'vectors')],check=True)
 vectors=list(struct.iter_unpack('<6f',(td/'vectors').read_bytes()))
 rng=random.Random(503);worst=0;cases=0;before=after=0
 for original,new,prefix in [('pddi_unlit_projection.v.pica','pddi_directional.v.pica','u_'),('skin_eye_packed.v.pica','skin_eye_directional.v.pica','u_eye_')]:
  old=(SHADERS/original).read_text();opt=(SHADERS/new).read_text()
  # Every instruction outside the light-loop specialization is unchanged.
  a=opt.index('\tifu u_directionalOnly\n');b=opt.index('\n\tdp3\tr6.x',a)
  restored=opt[:a]+opt[a:b].split('\t.else\n',1)[1].rsplit('\n\t.end\n',1)[0]+opt[b:]
  restored=restored.replace('\tifu u_directionalOnly\n\t.else\n\tmul\tr6.x, r6.x, r5.x\n\t.end','\tmul\tr6.x, r6.x, r5.x').replace('.bool u_directionalOnly\n','')
  assert restored==old
  h=td/(new+'.h');binary=td/(new+'.shbin')
  subprocess.run(['/opt/devkitpro/tools/bin/picasso','-o',str(binary),'-h',str(h),str(SHADERS/new)],check=True)
  assert '#define VSH_FLAG_u_directionalOnly BIT(2)' in h.read_text()
  if prefix=='u_eye_':assert '#define VSH_FVEC_u_bones 0x14' in h.read_text() and '#define VSH_FVEC_u_eye_quant 0x5F' in h.read_text()
  for case in range(6000):
   active=case%5;directional=case%3!=0
   normal=[f24(rng.uniform(.1,1)) for _ in range(3)]+[0]
   uniforms={'in_normal':normal,'r5':normal,'r8':[f24(rng.uniform(-50,50)) for _ in range(3)]+[1],prefix+'lightPos':[],prefix+'lightColour':[],prefix+'lightAtten':[]}
   for row in range(3):uniforms[f'u_mv[{row}]']=[1.0 if row==i else 0.0 for i in range(4)]
   for name in ['matEmissive','sceneAmbient','matAmbient','matDiffuse']:uniforms[prefix+name]=[f24(rng.random()) for _ in range(4)]
   prepared=[]
   for i in range(4):
    raw=vectors[(case*4+i)%len(vectors)]
    point=not directional and i==0
    uniforms[prefix+'lightPos'].append([f24(v) for v in raw[:3]]+[1 if point else 0])
    prepared.append([f24(v) for v in raw[3:]]+[0])
    uniforms[prefix+'lightColour'].append([f24(rng.random()) for _ in range(3)]+[1])
    uniforms[prefix+'lightAtten'].append([.9,.1,.01,0] if point else [1,0,0,0])
   expected,ops=run(old,uniforms,active,False)
   if directional:uniforms[prefix+'lightPos']=prepared
   actual,newops=run(opt,uniforms,active,directional)
   if not directional:assert expected==actual,'General light arithmetic changed'
   error=max(abs(x-y) for x,y in zip(expected,actual));worst=max(worst,error);assert error<1/255,(case,error)
   assert actual[3]==expected[3]
   if directional:before+=ops;after+=newops
   cases+=1
 print(f'PASS: {cases} production float24 lighting cases; general/mixed lights exact, directional worst color error={worst:.9f}; arithmetic instructions {before} -> {after} (branch cost excluded; not FPS)')
