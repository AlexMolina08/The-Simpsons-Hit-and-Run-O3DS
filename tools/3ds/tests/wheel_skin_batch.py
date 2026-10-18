#!/usr/bin/env python3
"""Check live wheel submission through the existing packed rigid skin path."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
a=s.index('bool picaContext::DrawWheelSkin(');b=s.index('{',a)+1;end=b;depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
method=s[a:end]
a=s.index('            picaSkin::Source rigid={};');b=s.index('\n#elif defined(SRR2_OLD3DS_WHEEL_GPU_BATCH)',a)
pack=s[a:b].replace('{delete result;return nullptr;}','return false;').replace('result->buffer+(copy*allocated+v)*stride','destination')
code=r'''
#define SRR2_OLD3DS_GPU_SKIN_PACKED
#define SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
#define SRR2_OLD3DS_GPU_SKIN_BATCH
#include <pddi/pica/wheellod.hpp>
#include <pddi/pica/skincore.hpp>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <random>
#include <limits>
struct pddiMatrix{float m[4][4]={};const float* Row4(unsigned r)const{return m[r];}};
struct pddiShader{};
struct picaPrimBuffer{bool wheelSkinBuffer=true,skinDrawing=false;};
struct C3D_FVec{float w,z,y,x;};
C3D_FVec uploaded[12];unsigned uniformCalls=0,bindCalls=0,draws=0;
const int GPU_VERTEX_SHADER=0,PDDI_MATRIX_MODELVIEW=0;
bool g_picaInFrame=true,enabled=true;
bool picaSkinEnabled(){return enabled;}
struct {unsigned vectors=0;}g_picaSkinMetrics;
C3D_FVec* C3D_FVUnifWritePtr(int,unsigned first,unsigned count){assert(first==20 && count==12);++uniformCalls;return uploaded;}
struct Stack{pddiMatrix view;const pddiMatrix* Top()const{return &view;}};
struct picaContext{
 bool bottomPass=false,skinProgramBound=false,skinBinary=true,rigid=false;unsigned skinBatchDepth=0;
 struct{Stack* matrixStack[1];}state;
 void SetSkinLayout(bool r){rigid=r;}
 bool BindSkinProgram(bool e){++bindCalls;skinProgramBound=e;return true;}
 void DrawPrimBuffer(pddiShader*,picaPrimBuffer* p){assert(skinProgramBound && rigid && p->skinDrawing && p->wheelSkinBuffer);++draws;}
 bool DrawWheelSkin(pddiShader*,picaPrimBuffer*,const pddiMatrix*);
};
bool packWheel(void* destination,const picaWheelLOD::Vertex& source,unsigned copy){const unsigned copies=4;
'''+pack+r'''
return true;}
'''+method+r'''
int main(){
 std::mt19937 rng(8105);auto value=[&](float scale){return (int(rng()%32768)-16384)*scale;};
 unsigned vertices=0,submissions=0;
 for(unsigned trial=0;trial<12000;++trial){
  picaWheelLOD::Vertex source;
  for(unsigned i=0;i<3;++i){source.position[i]=value(1.0f/8192);source.normal[i]=value(1.0f/16384);}
  for(float& v:source.uv)v=value(1.0f/4096);
  for(unsigned char& v:source.colour)v=rng();
  for(unsigned wheel=0;wheel<4;++wheel){
   picaSkin::PackedRigidVertex packed;assert(packWheel(&packed,source,wheel));
   assert(!memcmp(packed.colour,source.colour,4));assert(packed.offsets[0]==int(3*wheel) && packed.offsets[3]==-1);
   for(unsigned i=0;i<3;++i){assert(std::fabs(packed.position[i]/8192.0f-source.position[i])<=1.0f/16384);assert(std::fabs(packed.normal[i]/32768.0f-source.normal[i])<=1.0f/32768);}
   for(unsigned i=0;i<2;++i)assert(std::fabs(packed.uv[i]/4096.0f-source.uv[i])<=1.0f/8192);
   ++vertices;
  }
  auto invalid=source;invalid.position[0]=std::numeric_limits<float>::infinity();picaSkin::PackedRigidVertex packed;
  assert(!packWheel(&packed,invalid,0));invalid=source;invalid.uv[0]=9;assert(!packWheel(&packed,invalid,0));
  picaContext context;Stack stack;context.state.matrixStack[0]=&stack;pddiMatrix wheels[4];
  for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)stack.view.m[r][c]=r<3&&c<3?value(.0001f):0;
  stack.view.m[3][3]=1;for(unsigned c=0;c<3;++c)stack.view.m[3][c]=value(.01f);
  for(auto& m:wheels){for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)m.m[r][c]=value(.0001f);m.m[3][3]=1;for(unsigned c=0;c<3;++c)m.m[3][c]=value(.01f);}
  picaPrimBuffer buffer;unsigned before=draws;assert(context.DrawWheelSkin(nullptr,&buffer,wheels));assert(draws==before+1 && !context.skinProgramBound && !buffer.skinDrawing);++submissions;
  for(unsigned i=0;i<4;++i)for(unsigned r=0;r<3;++r){
   float ref[4];for(unsigned c=0;c<3;++c)ref[c]=wheels[i].m[c][0]*stack.view.m[0][r]+wheels[i].m[c][1]*stack.view.m[1][r]+wheels[i].m[c][2]*stack.view.m[2][r];
   ref[3]=wheels[i].m[3][0]*stack.view.m[0][r]+wheels[i].m[3][1]*stack.view.m[1][r]+wheels[i].m[3][2]*stack.view.m[2][r]+stack.view.m[3][r];
   assert(uploaded[3*i+r].x==ref[0] && uploaded[3*i+r].y==ref[1] && uploaded[3*i+r].z==ref[2] && uploaded[3*i+r].w==ref[3]);
  }
  before=uniformCalls;unsigned binds=bindCalls;
  context.bottomPass=true;assert(!context.DrawWheelSkin(nullptr,&buffer,wheels));context.bottomPass=false;
  context.skinBatchDepth=1;assert(!context.DrawWheelSkin(nullptr,&buffer,wheels));context.skinBatchDepth=0;
  enabled=false;assert(!context.DrawWheelSkin(nullptr,&buffer,wheels));enabled=true;
  buffer.wheelSkinBuffer=false;assert(!context.DrawWheelSkin(nullptr,&buffer,wheels));buffer.wheelSkinBuffer=true;
  wheels[3].m[0][0]=std::numeric_limits<float>::quiet_NaN();assert(!context.DrawWheelSkin(nullptr,&buffer,wheels));
  assert(uniformCalls==before && bindCalls==binds && !buffer.skinDrawing);
 }
 printf("PASS: %u production rigid wheel vertices and %u live palette submissions; quantization bounds, WZYX uploads, restore and pre-mutation fallbacks. Existing shader unchanged; not hardware performance proof.\n",vertices,submissions);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-wheel-skin-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
