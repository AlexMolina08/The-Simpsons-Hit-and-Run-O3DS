#!/usr/bin/env python3
"""Compare production material setters/preparation/submission against reference."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/pddi/pica/material.cpp').read_text()
header=(root/'libs/pure3d/pddi/pica/material.hpp').read_text()
header='\n'.join(x for x in header.splitlines() if not x.startswith('#include'))
source=source[source.index('static const GPU_TEXTURE_FILTER_PARAM'):source.index('#ifdef SRR2_OLD3DS_MATERIAL_QUEUE\nbool')]
prelude=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <vector>
using GPU_TEXTURE_FILTER_PARAM=int; using GPU_TEXTURE_WRAP_PARAM=int;
using GPU_TESTFUNC=int; using GPU_BLENDEQUATION=int; using GPU_BLENDFACTOR=int;
using pddiUVMode=int; using pddiFilterMode=int; using pddiShadeMode=int;
using pddiBlendMode=int; using pddiCompareMode=int; using pddiTextureGen=int;
using pddiShadeColourTable=int;using pddiShadeTextureTable=int;using pddiShadeIntTable=int;using pddiShadeFloatTable=int;
struct pddiColour {
 unsigned c=0;
 void Set(unsigned r,unsigned g,unsigned b){c=(255u<<24)|(b<<16)|(g<<8)|r;}
 void SetAlpha(unsigned a){c=(c&0xffffff)|(a<<24);}
 unsigned Red()const{return c&255;}unsigned Green()const{return(c>>8)&255;}
 unsigned Blue()const{return(c>>16)&255;}unsigned Alpha()const{return c>>24;}
};
struct C3D_Tex {int filter=0,wrap=0,id=0;};
struct pddiTexture {int refs=0;void AddRef(){++refs;}void Release(){--refs;}};
struct picaTexture: pddiTexture {C3D_Tex texture;bool valid=true;C3D_Tex* GetC3DTexture(){return valid?&texture:nullptr;}};
struct pddiBaseShader {int lastError=0;};
struct {int env0Mode=-1;bool passStagesSet=false;} g_picaState;
bool g_picaInFrame=true;
static std::vector<unsigned> trace;
static unsigned conversions=0;
void record(float f){unsigned v;memcpy(&v,&f,4);trace.push_back(v);}
struct picaContext {
 void EnableCulling(bool b){trace.push_back(b);}
 void SetMaterialWriteMask(int m){trace.push_back(m);}
 void SetVertexUniforms(bool lit,pddiColour a,pddiColour d,pddiColour e,const float (*p)[4]=nullptr){
  trace.push_back(lit);if(!lit)return;
  pddiColour c[]={a,d,e};
  for(unsigned i=0;i<3;++i){
   float expected[]={float(c[i].Red())/255.0f,float(c[i].Green())/255.0f,float(c[i].Blue())/255.0f,i==0?1.0f:i==1?float(d.Alpha())/255.0f:0.0f};
   if(p)assert(!memcmp(p[i],expected,sizeof(expected)));else ++conversions;
   for(auto v:expected)record(v);
  }
 }
};
void C3D_TexSetFilter(C3D_Tex*t,int a,int b){assert(a==b);t->filter=a;}
void C3D_TexSetWrap(C3D_Tex*t,int a,int b){assert(a==b);t->wrap=a;}
void picaTexBind0(C3D_Tex*t){trace.push_back(t->id);trace.push_back(t->filter);trace.push_back(t->wrap);}
struct C3D_TexEnv{};
C3D_TexEnv* C3D_GetTexEnv(int){static C3D_TexEnv e;return &e;}
void C3D_TexEnvInit(C3D_TexEnv*){}
void C3D_TexEnvSrc(C3D_TexEnv*,int,int,int,int){}
void C3D_TexEnvFunc(C3D_TexEnv*,int,int){}
void picaAlphaTest(bool b,int c,int r){trace.push_back(b);trace.push_back(c);trace.push_back(r);}
void picaAlphaBlend(int a,int b,int c,int d,int e,int f){for(int v:{a,b,c,d,e,f})trace.push_back(v);}
'''
names=sorted(set(re.findall(r'\b(?:GPU_|C3D_Both|PDDI_)[A-Za-z0-9_]*',source+header))-set(['PDDI_PICA_MATERIAL_HPP']))
names=[n for n in names if n not in ['GPU_TEXTURE_FILTER_PARAM','GPU_TEXTURE_WRAP_PARAM','GPU_TESTFUNC','GPU_BLENDEQUATION','GPU_BLENDFACTOR']]
prelude+='\nenum {'+','.join(n+'='+str(i) for i,n in enumerate(names))+'};\n'
# Default modes must address their actual tables.
for key,value in [('PDDI_UV_CLAMP',1),('PDDI_FILTER_BILINEAR',1),('PDDI_COMPARE_GREATEREQUAL',5),('PDDI_BLEND_NONE',0)]:
 prelude=re.sub(r'\b'+key+r'=\d+',key+'='+str(value),prelude)
tests=r'''
int main(){
 picaContext context;picaTexture textures[2];textures[0].texture.id=100;textures[1].texture.id=101;
 {
 picaMat a(&context),b(&context);picaMat* mats[]={&a,&b};
 uint32_t rng=42;auto next=[&](){rng=rng*1664525u+1013904223u;return rng;};
 for(unsigned n=0;n<10000;++n){
  picaMat& m=*mats[n%2];pddiColour c;c.c=next();
  switch(n%13){
   case 0:m.SetAmbient(c);break;case 1:m.SetDiffuse(c);break;case 2:m.SetEmissive(c);break;
   case 3:m.SetEmissiveAlpha(next()%256);break;case 4:m.SetTexture(&textures[next()%2]);break;
   case 5:m.SetUVMode(next()%3);m.SetFilterMode(next()%5);break;
   case 6:m.EnableAlphaTest(next()%2);m.SetAlphaRef(float(int(next()%1000)-400)/255.0f);break;
   case 7:m.SetAlphaCompare(next()%8);m.SetBlendMode(next()%8);break;
   case 8:m.SetTwoSided(next()%2);m.SetColourWrite(next()%16);break;
   case 9:m.EnableLighting(next()%2);break;case 10:m.SetTexture(nullptr);break;
   case 11:g_picaState.env0Mode=-1;g_picaState.passStagesSet=false;break;
   case 12:textures[0].valid=!textures[0].valid;break;
  }
  g_picaInFrame=n%19!=0;m.SetDevPass(0);m.PreRender();
  // Repeated submission and mutations share one frame, with no identity shortcut.
  m.PreRender();m.SetDiffuse(c);m.PreRender();
 }
 }
 assert(textures[0].refs==0&&textures[1].refs==0);
 uint64_t hash=1469598103934665603ull;for(auto v:trace){hash^=v;hash*=1099511628211ull;}
 printf("%llu %zu\n",(unsigned long long)hash,trace.size());
#ifdef SRR2_OLD3DS_DRAW_PREP
 assert(conversions==0);
 { picaMat m(&context);g_picaInFrame=true;m.EnableLighting(0);
   auto same=[&](){m.SetUVMode(1);m.SetFilterMode(1);m.SetTwoSided(0);m.SetBlendMode(0);
     m.EnableAlphaTest(0);m.SetAlphaCompare(5);m.SetAlphaRef(0.5f);m.SetColourWrite(15);};
   same();m.SetDevPass(0);auto before=trace.size();same();m.PreRender();
   assert(trace.size()==before+1); // Only the live uniform call; no material state replay.
 }

#endif
}
'''
with tempfile.TemporaryDirectory(prefix='srr-draw-prep-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+header+'\n'+source+tests)
 outputs=[]
 for mode in ['reference','prepared']:
  cmd=['clang++','-std=c++14','-O2','-Wall','-Wextra','-Wno-unused-parameter','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/mode)]
  if mode=='prepared':cmd+=['-DSRR2_OLD3DS_DRAW_PREP']
  subprocess.run(cmd,check=True);outputs.append(subprocess.check_output([str(p/mode)]))
 assert outputs[0]==outputs[1],outputs
 print('PASS: 10,000 production material mutation/target/texture cases; exact native state and colour output; no submission-time colour conversion in prepared path')
