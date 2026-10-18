#!/usr/bin/env python3
"""Check wheel palette bytes, live matrix uploads and native layout transitions."""
from pathlib import Path
import subprocess,tempfile,struct
root=Path(__file__).resolve().parents[3]
s=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
def extract(signature):
 a=s.index(signature);b=s.index('{',a)+1;end=b;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]
methods='\n'.join(extract(x) for x in ['void picaContext::SetWheelPaletteLayout','bool picaContext::DrawWheelPalette'])
code=r'''
#include <pddi/pica/wheellod.hpp>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
#include <cstddef>
#define SRR2_OLD3DS_NO_NORMAL_FETCH
#define SRR2_OLD3DS_GPU_SKIN
const int GPU_VERTEX_SHADER=0,VSH_FVEC_u_wheelMV=28;
struct pddiMatrix {
 float m[4][4]={};const float* Row4(unsigned r)const{return m[r];}
 void Mult(const pddiMatrix&a,const pddiMatrix&b){for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)m[r][c]=a.m[r][0]*b.m[0][c]+a.m[r][1]*b.m[1][c]+a.m[r][2]*b.m[2][c]+a.m[r][3]*b.m[3][c];}
};
struct FVec{float w,z,y,x;};struct C3D_Mtx{FVec r[4];};
struct C3D_AttrInfo {unsigned id;};
struct pddiShader{};struct picaPrimBuffer{bool wheelPaletteBuffer=true;};
struct State{bool bufInfoSet=true;}g_picaState;
bool g_picaInFrame=true;unsigned draws=0;bool paletteEnabled=false;const C3D_AttrInfo* currentLayout=nullptr;
std::vector<C3D_Mtx> uploads;
void C3D_SetAttrInfo(const C3D_AttrInfo* p){currentLayout=p;}
void C3D_BoolUnifSet(int,unsigned id,bool enabled){assert(id==0x6b);paletteEnabled=enabled;}
void C3D_FVUnifMtx4x4(int,unsigned id,const C3D_Mtx*p){assert(id==28+uploads.size()*4);uploads.push_back(*p);}
struct Stack{pddiMatrix view;const pddiMatrix* Top()const{return &view;}};
const unsigned PDDI_MATRIX_MODELVIEW=0;
struct picaContext {
 bool bottomPass=false,skinProgramBound=false,wheelPaletteLayout=false,noNormalLayout=false;
 C3D_AttrInfo attrInfo{1},wheelPaletteAttrInfo{2},noNormalAttrInfo{3};
 struct{Stack* matrixStack[1];}state;
 void DrawPrimBuffer(pddiShader*,picaPrimBuffer*){assert(paletteEnabled);++draws;}
 bool DrawWheelPalette(pddiShader*,picaPrimBuffer*,const pddiMatrix*);
 void SetWheelPaletteLayout(bool);
};
'''+methods+r'''
int main(){
 static_assert(offsetof(picaWheelLOD::PaletteVertex,normal)==12,"normal layout");
 static_assert(offsetof(picaWheelLOD::PaletteVertex,uv)==28,"UV layout");
 static_assert(offsetof(picaWheelLOD::PaletteVertex,colour)==36,"color layout");
 unsigned rng=771;auto word=[&](){rng=rng*1664525u+1013904223u;return rng;};
 unsigned cases=0;
 for(unsigned trial=0;trial<12000;++trial){
  picaWheelLOD::Vertex source;unsigned char* bytes=reinterpret_cast<unsigned char*>(&source);for(unsigned i=0;i<sizeof(source);++i)bytes[i]=word()>>24;
  for(unsigned wheel=0;wheel<4;++wheel){auto packed=picaWheelLOD::Palette(source,wheel);
   assert(!memcmp(packed.position,source.position,12));assert(!memcmp(packed.normal,source.normal,12));
   assert(!memcmp(packed.uv,source.uv,8));assert(!memcmp(packed.colour,source.colour,4));assert(packed.normal[3]==wheel*4);++cases;
  }
  picaContext context;Stack stack;context.state.matrixStack[0]=&stack;pddiMatrix wheels[4];
  for(auto& wheel:wheels)for(auto& row:wheel.m)for(float& v:row)v=float(int(word()%2048)-1024)/128;
  for(auto& row:stack.view.m)for(float& v:row)v=float(int(word()%2048)-1024)/128;
  uploads.clear();assert(context.DrawWheelPalette(nullptr,new picaPrimBuffer,wheels));
  // Citro3D vectors physically store W,Z,Y,X. Compare every uploaded word.
  for(unsigned wheel=0;wheel<4;++wheel){pddiMatrix expected;expected.Mult(wheels[wheel],stack.view);
   for(unsigned row=0;row<4;++row){assert(uploads[wheel].r[row].x==expected.m[0][row]);assert(uploads[wheel].r[row].y==expected.m[1][row]);assert(uploads[wheel].r[row].z==expected.m[2][row]);assert(uploads[wheel].r[row].w==expected.m[3][row]);}}
  assert(!paletteEnabled);
  picaPrimBuffer buffer;
  context.bottomPass=true;assert(!context.DrawWheelPalette(nullptr,&buffer,wheels));context.bottomPass=false;
  context.skinProgramBound=true;assert(!context.DrawWheelPalette(nullptr,&buffer,wheels));context.skinProgramBound=false;
  g_picaInFrame=false;assert(!context.DrawWheelPalette(nullptr,&buffer,wheels));g_picaInFrame=true;
  buffer.wheelPaletteBuffer=false;assert(!context.DrawWheelPalette(nullptr,&buffer,wheels));
 }
 picaContext context;
 context.SetWheelPaletteLayout(true);assert(currentLayout==&context.wheelPaletteAttrInfo&&!g_picaState.bufInfoSet);
 context.noNormalLayout=true;context.SetWheelPaletteLayout(false);assert(currentLayout==&context.noNormalAttrInfo);
 context.SetWheelPaletteLayout(true);context.noNormalLayout=false;context.SetWheelPaletteLayout(false);assert(currentLayout==&context.attrInfo);
 printf("PASS: %u production palette vertices and 12000 live matrix uploads; Citro3D WZYX packing, exact source attributes, state restoration and guarded fallback. Not native gains.\n",cases);
}
'''
code=code.replace('uploads.clear();assert(context.DrawWheelPalette(nullptr,new picaPrimBuffer,wheels));','uploads.clear();picaPrimBuffer drawable;assert(context.DrawWheelPalette(nullptr,&drawable,wheels));')
with tempfile.TemporaryDirectory(prefix='srr-wheel-gpu-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 header=p/'shader.h';binary=p/'shader.shbin'
 subprocess.run(['/opt/devkitpro/tools/bin/picasso','-o',str(binary),'-h',str(header),str(root/'libs/pure3d/pddi/pica/shaders/pddi_wheel_palette.v.pica')],check=True)
 data=binary.read_bytes();off=data.index(b'DVLP');instructions=struct.unpack_from('<I',data,off+12)[0];descriptors=struct.unpack_from('<I',data,off+20)[0]
 assert instructions<=512 and descriptors<=128
 h=header.read_text();assert '#define VSH_FLAG_u_wheelPalette BIT(3)' in h and '#define VSH_ULEN_u_wheelMV 16' in h
 print(f'PASS: production shader assembles: {instructions} instructions, {descriptors} descriptors; four-matrix palette register contract')

# Execute position and normal source instructions with float24-rounded operations.
# The unchanged lighting suffix consumes these same eye-space values.
import math,random,re
standard=(root/'libs/pure3d/pddi/pica/shaders/pddi_directional.v.pica').read_text()
candidate=(root/'libs/pure3d/pddi/pica/shaders/pddi_wheel_palette.v.pica').read_text()
assert standard.split('\tdp3\tr2.x')[1:]==candidate.split('\tdp3\tr2.x')[1:]
def f24(x):
 if not x:return x
 m,e=math.frexp(x);return math.ldexp(round(m*131072)/131072,e)
def instructions(source,proc,end):return source.split('.proc '+proc+'\n')[1].split(end)[0].strip().splitlines()
def execute(lines,uniforms,position,normal,fast,palette,address=0):
 registers={f'r{i}':[0.0]*4 for i in range(16)}
 registers.update(uniforms);registers.update(in_pos=position,in_normal=normal,ones=[1.0]*4,a0=[float(address),0.0,0.0,0.0],o_pos=[0.0]*4)
 def read(arg):
  sign=-1 if arg.startswith('-') else 1;arg=arg.lstrip('-')
  match=re.fullmatch(r'(\w+)(?:\[([^]]+)\])?(?:\.([xyzwrgba]+))?',arg);assert match,arg
  name,index,swizzle=match.groups();value=registers[name]
  if index is not None:
   if index.startswith('a0.x'):index=int(registers['a0'][0])+int(index[4:] or 0)
   else:index=int(index)
   value=value[index]
  swizzle=(swizzle or 'xyzw').translate(str.maketrans('rgba','xyzw'));swizzle=(swizzle+swizzle[-1]*4)[:4]
  return [sign*value['xyzw'.index(c)] for c in swizzle]
 def run(block):
  index=0
  while index<len(block):
   line=block[index].strip();index+=1
   if not line:continue
   if line.startswith('ifu '):
    start=index;depth=1;otherwise=None
    while depth:
     token=block[index].strip()
     if token.startswith('ifu '):depth+=1
     elif token=='.end':depth-=1
     elif token=='.else' and depth==1:otherwise=index
     index+=1
    stop=index-1;condition=fast if line=='ifu u_fastUnlit' else palette
    if condition:run(block[start:otherwise if otherwise is not None else stop])
    elif otherwise is not None:run(block[otherwise+1:stop])
    continue
   op,args=line.split(None,1);args=[x.strip() for x in args.split(',')]
   a=read(args[1]);b=read(args[2]) if len(args)>2 else None
   if op=='mova':value=[float(int(x)) for x in a]
   elif op=='mov':value=a
   elif op=='dp4':value=[f24(sum(f24(a[i]*b[i]) for i in range(4)))]*4
   elif op=='dp3':value=[f24(sum(f24(a[i]*b[i]) for i in range(3)))]*4
   elif op=='mul':value=[f24(a[i]*b[i]) for i in range(4)]
   elif op=='add':value=[f24(a[i]+b[i]) for i in range(4)]
   else:raise AssertionError(op)
   name,_,mask=args[0].partition('.')
   for c in mask or 'xyzw':i='xyzw'.index(c);registers[name][i]=f24(value[i])
 run(lines);return registers
old_position=instructions(standard,'vmain','\tmov\to_tex0')
new_position=instructions(candidate,'vmain','\tmov\to_tex0')
old_normal=instructions(standard,'DoLighting','\tdp3\tr2.x')
new_normal=instructions(candidate,'DoLighting','\tdp3\tr2.x')
rng=random.Random(7702);count=0
for case in range(3000):
 def vector():return [f24(rng.uniform(-2,2)) for _ in range(4)]
 palettes=[vector() for _ in range(16)];uniforms={'u_mv':[vector() for _ in range(4)],'u_wheelMV':palettes,'u_proj':[vector() for _ in range(4)],'u_unlitScale':vector(),'u_eye':vector(),'u_stereoOn':[float(case%2)]*4}
 position=vector();normal=vector()
 for fast in (False,True):
  # Ordinary draws retain the old program's position and normal computation.
  a=execute(old_position,uniforms,position,normal,fast,False);b=execute(new_position,uniforms,position,normal,fast,False)
  assert a['o_pos']==b['o_pos'] and a['r8']==b['r8']
  for wheel in range(4):
   reference=dict(uniforms,u_mv=palettes[wheel*4:wheel*4+4]);input_normal=normal[:3]+[float(wheel*4)]
   a=execute(old_position,reference,position,normal,fast,False);b=execute(new_position,uniforms,position,input_normal,fast,True)
   assert a['o_pos']==b['o_pos'] and a['r8']==b['r8']
   address=b['a0'][0]
   a=execute(old_normal,reference,position,normal,fast,False);b=execute(new_normal,uniforms,position,input_normal,fast,True,address)
   assert a['r9']==b['r9'];count+=1
print(f'PASS: {count} float24-rounded palette position/normal comparisons, regular draws, sparse/full projection and stereo correction; unchanged lighting suffix. Not PICA timing or rasterization proof.')
