#!/usr/bin/env python3
"""Compare the owned material executor against production SetDevPass and cache helpers."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
def clean(s):return '\n'.join(x for x in s.splitlines() if not x.startswith('#include'))+'\n'
def extract(s,signature):
 a=s.index(signature);b=s.index('{',a);end=b+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]+'\n'
material=(root/'libs/pure3d/pddi/pica/material.cpp').read_text()
cache=(root/'libs/pure3d/pddi/pica/statecache.hpp').read_text()
prepared=(root/'libs/pure3d/pddi/pica/preparedmaterial.hpp').read_text()
methods=''.join(extract(material,x) for x in ['void picaMat::PrepareNative(', 'void picaMat::SetDevPass(', 'bool picaMat::CapturePreparedPass('])
tables=material[material.index('static const GPU_TEXTURE_FILTER_PARAM'):material.index('#ifdef SRR2_OLD3DS_MATERIAL_WRITES')]
source=r'''
#include <vector>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
using u32=uint32_t;using u16=uint16_t;
#define SRR2_OLD_3DS
#define SRR2_OLD3DS_DRAW_PREP
#define SRR2_OLD3DS_PREPARED_BACKEND
'''
types=['GPU_TEXTURE_FILTER_PARAM','GPU_TEXTURE_WRAP_PARAM','GPU_TESTFUNC','GPU_BLENDEQUATION','GPU_BLENDFACTOR','GPU_CULLMODE','GPU_WRITEMASK','GPU_TEXCOLOR']
source+=''.join('using '+x+'=int;\n' for x in types)
constants=sorted(set(re.findall(r'\bGPU_[A-Z0-9_]+',tables+methods+cache+prepared))-set(types))
source+='enum {'+','.join(x+'='+str(i+1) for i,x in enumerate(constants))+'};\n'
source+=r'''
enum {C3D_Both=0,O3_MATERIAL_PASS,O3_STATE_CHANGE,O3_TEX_REQUEST,O3_TEX_CHANGE,PDDI_BLEND_NONE=0};
void Old3dsCount(int){}
void picaPreparedBackendFence(){}
struct C3D_Tex{void* data;u32 param;u16 width,height;int fmt;};
struct C3D_TexEnv{int stage;};
struct GPU{std::vector<std::array<uintptr_t,8>> trace;C3D_TexEnv env[6]={{0},{1},{2},{3},{4},{5}};const C3D_Tex* bound=nullptr;};
GPU* active;
void C3D_TexSetFilter(C3D_Tex* t,int a,int b){t->param=(t->param&~0xffu)|unsigned(a&15)|unsigned((b&15)<<4);}
void C3D_TexSetWrap(C3D_Tex* t,int a,int b){t->param=(t->param&~0xff00u)|unsigned((a&15)<<8)|unsigned((b&15)<<12);}
void C3D_TexBind(int,C3D_Tex* t){active->bound=t;active->trace.push_back({0,uintptr_t(t->data),t->param,t->width,t->height,uintptr_t(t->fmt)});}
C3D_TexEnv* C3D_GetTexEnv(int i){return &active->env[i];}
void C3D_TexEnvInit(C3D_TexEnv* e){active->trace.push_back({1,uintptr_t(e->stage)});}
void C3D_TexEnvSrc(C3D_TexEnv* e,int,int a,int b,int c){active->trace.push_back({2,uintptr_t(e->stage),uintptr_t(a),uintptr_t(b),uintptr_t(c)});}
void C3D_TexEnvFunc(C3D_TexEnv* e,int,int a){active->trace.push_back({3,uintptr_t(e->stage),uintptr_t(a)});}
void C3D_AlphaTest(bool e,int f,int r){active->trace.push_back({4,e,uintptr_t(f),uintptr_t(r)});}
void C3D_AlphaBlend(int a,int b,int c,int d,int e,int f){active->trace.push_back({5,uintptr_t(a),uintptr_t(b),uintptr_t(c),uintptr_t(d),uintptr_t(e),uintptr_t(f)});}
void C3D_CullFace(int){}void C3D_DepthTest(bool,int,int){}
'''
source+=clean((root/'libs/pure3d/pddi/pica/preparedbackend.h').read_text())+'bool picaPreparedCaptureState(unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned){return false;}\n'+clean(cache)
source+='picaStateCache g_picaState={};\n'+clean(prepared)+tables
source+=r'''
bool g_picaInFrame=true;
struct picaMat;
struct picaPreparedMaterialRaster{};
struct picaPreparedDrawInputs{picaPreparedMaterialRaster raster;const picaMat* materialOwner=nullptr;picaPreparedMaterialPass material;bool hasMaterial=false;};
struct picaContext{void CapturePreparedMaterialRaster(const picaPreparedMaterialPass&,picaPreparedMaterialRaster&){}picaPreparedDrawInputs* preparedDrawCapture=nullptr;bool culling=false;int mask=0;void EnableCulling(bool b){culling=b;active->trace.push_back({6,b});}void SetMaterialWriteMask(int i){mask=i;active->trace.push_back({7,uintptr_t(i)});}};
struct Texture{C3D_Tex tex={};bool valid=true;C3D_Tex* GetC3DTexture(){return valid?&tex:nullptr;}};
struct picaTextureEnv{Texture* texture=nullptr;int filterMode=0,uvMode=0,alphaCompareMode=0,alphaBlendMode=0,writeMask=0;bool alphaTest=false,twoSided=false;float alphaRef=0;};
struct picaMat{
 picaTextureEnv texEnv[1];picaContext* context;bool dirty=true,nativeDirty=true;
 int nativeFilter,nativeWrap,nativeCompare,nativeEquation,nativeSource,nativeDestination,nativeAlphaRef;
 void PrepareNative();void SetDevPass(unsigned);bool CapturePreparedPass(picaPreparedMaterialPass&);
};
'''
source+=methods
source+=r'''
unsigned seed=731;unsigned randomValue(){seed=1664525u*seed+1013904223u;return seed;}
int main(){
 picaContext referenceContext;picaMat reference,producer;reference.context=&referenceContext;
 Texture textures[3];GPU expected,actual;picaStateCache consumer={};picaPreparedMaterialCounters counts;
 // Records persist across cache hits: the SDK retains the last bound address.
 std::array<picaPreparedMaterialPass,50000> records;
 for(unsigned n=0;n<records.size();++n){
  if(n%37==0){g_picaState={};consumer={};}
  auto& e=reference.texEnv[0];
  if(n%3==0){e.texture=randomValue()%5?&textures[randomValue()%3]:nullptr;e.filterMode=randomValue()%5;e.uvMode=randomValue()%3;e.alphaTest=randomValue()%2;e.alphaCompareMode=randomValue()%8;e.alphaBlendMode=randomValue()%8;e.alphaRef=float(int(randomValue()%400)-70)/255.f;e.twoSided=randomValue()%2;e.writeMask=randomValue()%16;reference.nativeDirty=true;}
  if(e.texture && n%7==0){auto& t=e.texture->tex;t.data=reinterpret_cast<void*>(uintptr_t(randomValue()|1u));t.param=randomValue();t.width=randomValue()%1024;t.height=randomValue()%1024;t.fmt=randomValue()%8;e.texture->valid=randomValue()%5!=0;}
  producer.texEnv[0]=e;producer.nativeDirty=true;
  g_picaInFrame=n%101!=0;
  active=&actual;assert(producer.CapturePreparedPass(records[n])==g_picaInFrame);assert(actual.trace.empty());
  active=&expected;reference.SetDevPass(0);
  active=&actual;
  if(g_picaInFrame){
   // Producer and material may change after capture. Metadata must be owned.
   const auto saved=producer.texEnv[0];producer.texEnv[0]={};
   picaApplyPreparedMaterialPass(records[n],consumer,counts);
   active->trace.push_back({6,!records[n].twoSided});active->trace.push_back({7,uintptr_t(records[n].writeMask)});
   producer.texEnv[0]=saved;
   assert(consumer.alphaTestValid==g_picaState.alphaTestValid);
   assert(consumer.alphaTestEnable==g_picaState.alphaTestEnable);
   assert(consumer.alphaTestFunc==g_picaState.alphaTestFunc);assert(consumer.alphaTestRef==g_picaState.alphaTestRef);
   assert(consumer.blendColorEq==g_picaState.blendColorEq);assert(consumer.blendAlphaEq==g_picaState.blendAlphaEq);
   assert(consumer.blendSrcClr==g_picaState.blendSrcClr);assert(consumer.blendDstClr==g_picaState.blendDstClr);
   assert(consumer.blendSrcAlpha==g_picaState.blendSrcAlpha);assert(consumer.blendDstAlpha==g_picaState.blendDstAlpha);
   assert(consumer.texPtr==g_picaState.texPtr);assert(consumer.texData==g_picaState.texData);assert(consumer.texParam==g_picaState.texParam);
   assert(consumer.texWidth==g_picaState.texWidth);assert(consumer.texHeight==g_picaState.texHeight);assert(consumer.texFmt==g_picaState.texFmt);
   assert(consumer.env0Mode==g_picaState.env0Mode);assert(consumer.passStagesSet==g_picaState.passStagesSet);
  }
  assert(expected.trace==actual.trace);expected.trace.clear();actual.trace.clear();
 }
 puts("PASS: 50000 captured material passes match production SetDevPass call order and cache state. Includes shared textures, sampler changes, invalid textures, alpha clamping, no-frame capture and cache invalidation. Mock SDK; not FPS or lifetime-fence proof.");
}
'''
with tempfile.TemporaryDirectory(prefix='prepared-material-') as d:
 p=Path(d);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
