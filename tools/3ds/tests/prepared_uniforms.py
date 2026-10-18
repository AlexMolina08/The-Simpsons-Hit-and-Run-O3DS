#!/usr/bin/env python3
"""Compare owned uniform inputs/executor with the actual legacy draw method."""
from pathlib import Path
import re,subprocess,tempfile,sys
root=Path(__file__).resolve().parents[3]
def clean(s):return '\n'.join(line for line in s.splitlines() if not line.startswith('#include'))+'\n'
def extract(s,signature):
 a=s.index(signature);b=s.index('{',a);end=b+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]+'\n'
source=r'''
#include <array>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <cmath>
#include <thread>
using u32=uint32_t;
#define BIT(n) (1u<<(n))
#define SRR2_OLD_3DS
#define SRR2_OLD3DS_PREPARED_BACKEND
#define SRR2_OLD3DS_GPU_SKIN
#define SRR2_OLD3DS_GPU_SKIN_SUPPORT
#define SRR2_OLD3DS_GPU_SKIN_COMPACT
#define SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
#define SRR2_OLD3DS_GPU_SKIN_PACKED
#define SRR2_OLD3DS_UNLIT_PROJECTION
#define SRR2_OLD3DS_DIRECTIONAL_LIGHT
#define SRR2_OLD3DS_LIGHT_LOOP
constexpr int PDDI_MAX_LIGHTS=4;constexpr unsigned PDDI_MATRIX_MODELVIEW=0,GPU_VERTEX_SHADER=0;
struct V4{float x,y,z,w;const float& operator[](unsigned i)const{return (&x)[i];}};
struct pddiMatrix{V4 rows[4];const V4& Row4(unsigned i)const{return rows[i];}};
struct C3D_FVec{float w,z,y,x;};struct C3D_Mtx{C3D_FVec r[4];};
struct pddiColour{u32 c=0;unsigned Red()const{return c&255;}unsigned Green()const{return (c>>8)&255;}unsigned Blue()const{return (c>>16)&255;}unsigned Alpha()const{return c>>24;}};
struct GPU{C3D_FVec vectors[96]={};u32 integers[4]={},booleans=0;std::vector<std::array<u32,6>> trace;};
thread_local GPU* active;
u32 bits(float value){u32 out;memcpy(&out,&value,4);return out;}
void C3D_FVUnifSet(unsigned,unsigned id,float x,float y,float z,float w){assert(id<96);active->vectors[id]={w,z,y,x};active->trace.push_back({0,id,bits(x),bits(y),bits(z),bits(w)});}
C3D_FVec* C3D_FVUnifWritePtr(unsigned,unsigned id,unsigned count){assert(id+count<=96);active->trace.push_back({1,id,count,0,0,0});return &active->vectors[id];}
void C3D_FVUnifMtx4x4(unsigned type,unsigned id,const C3D_Mtx* m){for(unsigned i=0;i<4;++i)C3D_FVUnifSet(type,id+i,m->r[i].x,m->r[i].y,m->r[i].z,m->r[i].w);}
void C3D_BoolUnifSet(unsigned,unsigned id,bool value){assert(id>=0x68 && id<0x78);unsigned mask=1u<<(id-0x68);if(value)active->booleans|=mask;else active->booleans&=~mask;active->trace.push_back({2,id,unsigned(value),0,0,0});}
void C3D_IVUnifSet(unsigned,unsigned id,unsigned x,unsigned y,unsigned z,unsigned w){assert(id>=0x60 && id<0x64);active->integers[id-0x60]=x|(y<<8)|(z<<16)|(w<<24);active->trace.push_back({3,id,x,y,z,w});}
'''
for name in ['pddi_shbin.h','skin_shbin.h']:
 source+='\n'.join(line for line in (Path('/tmp/srr2-whole-frame-ipo-build/libs/pure3d')/name).read_text().splitlines() if line.startswith('#define VSH_'))+'\n'
source+=clean((root/'libs/pure3d/pddi/pica/unlitprojection.hpp').read_text())+clean((root/'libs/pure3d/pddi/pica/directionallight.hpp').read_text())
source+=clean((root/'libs/pure3d/pddi/pica/prepareduniforms.hpp').read_text())
source+=r'''
unsigned g_old3dsModelViewUploads=0,g_old3dsModelViewReuses=0;bool g_picaInFrame=true;
enum {O3_UNIFORMS,O3_MV_UPLOAD,O3_MV_REUSE};void Old3dsCount(unsigned){}
struct Old3dsOperationScope{Old3dsOperationScope(unsigned,unsigned){}};
struct Light{bool enabled;C3D_FVec eye;pddiColour colour;float attenA,attenB,attenC;};
struct Lighting{pddiColour ambient;Light light[4];};
using pddiLight=Light;
struct Stack{pddiMatrix value;pddiMatrix* Top(){return &value;}};
struct Display{bool stereo;bool StereoActive(){return stereo;}};
struct picaContext:picaPreparedUniformCache{
 Stack stack;Lighting lighting;Display screen;
 struct {Stack* matrixStack[1];Lighting* lightingState;} state;
 Display* display;C3D_Mtx hwProjection;bool skinProgramBound=false,stereoOn=false,lightsDirty=true,directionalOnly=false;
 C3D_FVec eyeLight[4],lightPos[4],lightColour[4],lightAtten[4],directionalPos[4];int nActiveLights=0;
 unsigned preparedUniformEpoch=0,preparedLightEpoch=0,preparedProjectionEpoch=0;
 picaContext(){state.matrixStack[0]=&stack;state.lightingState=&lighting;display=&screen;}
 void RebuildLightArray();void InvalidateUniformCache();void FlushPreparedExecution(bool){}
 void SetVertexUniforms(bool,pddiColour,pddiColour,pddiColour,const float (*)[4]);
 bool CapturePreparedUniforms(picaPreparedUniformInput&,bool,pddiColour,pddiColour,pddiColour,const float (*)[4]);
};
'''
legacy=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
for sig in ['void picaContext::RebuildLightArray(', 'void picaContext::InvalidateUniformCache(', 'void picaContext::SetVertexUniforms(', 'bool picaContext::CapturePreparedUniforms(']:source+=extract(legacy,sig)
source+=r'''
unsigned seed=9171;unsigned randomValue(){seed=seed*1664525u+1013904223u;return seed;}
float scalar(){return float(int(randomValue()%20001)-10000)/100.f;}
int main(){
 picaContext reference,producer;picaPreparedUniformCache cache;GPU expected,actual;
 unsigned writes=0;
 for(unsigned trial=0;trial<40000;++trial){
  if(trial%5==0){for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)reference.stack.value.rows[r].operator[](c); // checked access below
   for(unsigned r=0;r<4;++r){reference.stack.value.rows[r]={scalar(),scalar(),scalar(),r==3?1.f:0.f};}producer.stack.value=reference.stack.value;}
  if(trial%17==0){for(auto& row:reference.hwProjection.r)row={scalar(),scalar(),scalar(),scalar()};producer.hwProjection=reference.hwProjection;++reference.preparedProjectionEpoch;++producer.preparedProjectionEpoch;reference.InvalidateUniformCache();producer.InvalidateUniformCache();}
  if(trial%31==0){reference.hwProjection={};reference.hwProjection.r[0].y=1;reference.hwProjection.r[1].x=1;reference.hwProjection.r[2].z=1;reference.hwProjection.r[2].w=-1;reference.hwProjection.r[3].z=1;producer.hwProjection=reference.hwProjection;++reference.preparedProjectionEpoch;++producer.preparedProjectionEpoch;reference.InvalidateUniformCache();producer.InvalidateUniformCache();}
  if(trial%43==0){reference.hwProjection.r[0].x=scalar();producer.hwProjection=reference.hwProjection;++reference.preparedProjectionEpoch;++producer.preparedProjectionEpoch;reference.projUploaded=producer.projUploaded=false;}
  if(trial%7==0){reference.skinProgramBound=producer.skinProgramBound=!reference.skinProgramBound;reference.InvalidateUniformCache();producer.InvalidateUniformCache();}
  reference.screen.stereo=producer.screen.stereo=trial%11!=0;reference.stereoOn=producer.stereoOn=trial%13!=0;
  if(trial%3==0){
   for(unsigned i=0;i<4;++i){auto& light=reference.lighting.light[i];light.enabled=bool(randomValue()&1);light.colour.c=randomValue();light.attenA=trial%29?scalar():0;light.attenB=trial%29?scalar():0;light.attenC=trial%29?scalar():0;
    reference.eyeLight[i]={trial%19?0.f:1.f,0,0,1};}
   memcpy(producer.lighting.light,reference.lighting.light,sizeof(reference.lighting.light));memcpy(producer.eyeLight,reference.eyeLight,sizeof(reference.eyeLight));reference.lightsDirty=producer.lightsDirty=true;
  }
  reference.lighting.ambient.c=producer.lighting.ambient.c=randomValue();
  pddiColour ambient,diffuse,emissive;ambient.c=randomValue();diffuse.c=randomValue();emissive.c=randomValue();
  const bool lit=trial%4!=0;float prepared[3][4];for(auto& row:prepared)for(auto& value:row)value=scalar();
  const float (*colours)[4]=trial%2?prepared:nullptr;
  expected.trace.clear();actual.trace.clear();active=&expected;reference.SetVertexUniforms(lit,ambient,diffuse,emissive,colours);
  picaPreparedUniformInput input;assert(producer.CapturePreparedUniforms(input,lit,ambient,diffuse,emissive,colours));
  picaPreparedUniformInput relocated;memcpy(&relocated,&input,sizeof(input));memset(&input,0x87,sizeof(input));memset(prepared,0x76,sizeof(prepared));
  const picaPreparedUniformView view(relocated);
  picaPreparedViewUniforms viewRecord={relocated.projection,relocated.stereo,relocated.stereoActive,relocated.projectionEpoch};
  picaPreparedLightUniforms lightRecord;
  memcpy(lightRecord.position,relocated.lightPosition,sizeof(lightRecord.position));memcpy(lightRecord.colour,relocated.lightColour,sizeof(lightRecord.colour));
  memcpy(lightRecord.attenuation,relocated.lightAttenuation,sizeof(lightRecord.attenuation));memcpy(lightRecord.directionalPosition,relocated.directionalPosition,sizeof(lightRecord.directionalPosition));
  lightRecord.sceneAmbient=relocated.sceneAmbient;lightRecord.epoch=relocated.lightEpoch;lightRecord.count=relocated.lightCount;lightRecord.directional=relocated.directional;
  picaPreparedMaterialUniforms materialRecord;materialRecord.ambient=relocated.ambient;materialRecord.diffuse=relocated.diffuse;materialRecord.emissive=relocated.emissive;
  memcpy(materialRecord.colours,relocated.colours,sizeof(materialRecord.colours));materialRecord.lit=relocated.lit;materialRecord.hasPreparedColours=relocated.hasPreparedColours;
  picaPreparedTransformUniforms transformRecord={relocated.modelView,relocated.invalidationEpoch,relocated.skin};
  const picaPreparedUniformView sharedView(viewRecord,lightRecord,materialRecord,transformRecord);
  (void)view;
#ifdef CONCURRENT_CAPTURE
  std::thread consumer([&]{active=&actual;picaApplyPreparedUniforms(sharedView,cache);});
  std::array<unsigned char,sizeof(producer)> saved;memcpy(saved.data(),&producer,sizeof(producer));memset(&producer,0x65,sizeof(producer));consumer.join();memcpy(&producer,saved.data(),sizeof(producer));
#else
  active=&actual;if(trial&1)picaApplyPreparedUniforms(sharedView,cache);else picaApplyPreparedUniforms(relocated,cache);
#endif
  assert(expected.trace==actual.trace);assert(expected.booleans==actual.booleans);
  assert(!memcmp(expected.vectors,actual.vectors,sizeof(expected.vectors)));assert(!memcmp(expected.integers,actual.integers,sizeof(expected.integers)));
  writes+=expected.trace.size();
 }
 printf("PASS: 40000 production captures/prepared uniform executions match legacy writes, order and register state; %u writes. Shader/target invalidation, matrix/light changes, lit/unlit, prepared colours and record relocation. Mock SDK; not FPS.\n",writes);
}
'''
# Remove an intentionally unused read from the matrix-generation loop.
source=source.replace('for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)reference.stack.value.rows[r].operator[](c); // checked access below','')
if '--emit-source' in sys.argv:
 print(source);sys.exit(0)
with tempfile.TemporaryDirectory(prefix='srr-prepared-uniforms-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(source)
 flags=['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-pthread','-fsanitize='+('thread' if '--tsan' in sys.argv else 'address,undefined')]
 if '--tsan' in sys.argv:flags+=['-DCONCURRENT_CAPTURE']
 subprocess.run([*flags,str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
