#!/usr/bin/env python3
"""Execute upstream and replacement uniform managers against one GPU-state model."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
def clean(path):
    return '\n'.join(line for line in path.read_text().splitlines()
                     if not line.startswith('#include')).replace('_Static_assert', 'static_assert')

prelude = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
using u32=uint32_t;using u16=uint16_t;
enum GPU_SHADER_TYPE{GPU_VERTEX_SHADER,GPU_GEOMETRY_SHADER};
const int C3D_FVUNIF_COUNT=96,C3D_IVUNIF_COUNT=4;
const unsigned GPUREG_VSH_BOOLUNIFORM=0x2b0,GPUREG_GSH_BOOLUNIFORM=0x280;
const unsigned GPUREG_VSH_FLOATUNIFORM_CONFIG=0x2c0,GPUREG_VSH_FLOATUNIFORM_DATA=0x2c1,GPUREG_VSH_INTUNIFORM_I0=0x2b1;
#define BIT(n) (1u<<(n))
struct C3D_FVec{u32 data[4];};using C3D_IVec=u32;
struct float24Uniform_s{u32 id,data[3];};struct DVLE{GPU_SHADER_TYPE type;};
struct shaderInstance_s{DVLE* dvle;u16 boolUniforms,boolUniformMask;u32 intUniforms[4];float24Uniform_s* float24Uniforms;unsigned intUniformMask,numFloat24Uniforms;};
'''
fixture = r'''
bool fast=true;bool nativeBenchNativeUniforms(){return fast;}
struct GPU{u32 vectors[2][96][5]={},integers[2][4]={},booleans[2]={};} gpu;
unsigned cursor[2]={};unsigned long long dataWords=0;
void GPUCMD_AddWrite(unsigned reg,u32 value){
 unsigned type=reg<0x2b0;unsigned offset=type?0x30:0;reg+=offset;
 if(reg==GPUREG_VSH_FLOATUNIFORM_CONFIG){cursor[type]=value&0x7f;return;}
 if(reg==GPUREG_VSH_BOOLUNIFORM){gpu.booleans[type]=value;return;}
 assert(reg>=GPUREG_VSH_INTUNIFORM_I0 && reg<GPUREG_VSH_INTUNIFORM_I0+4);
 gpu.integers[type][reg-GPUREG_VSH_INTUNIFORM_I0]=value;
}
void GPUCMD_AddWrites(unsigned reg,const u32* data,unsigned count){
 unsigned type=reg<0x2b0;assert(reg+(type?0x30:0)==GPUREG_VSH_FLOATUNIFORM_DATA && count%4==0);
 for(unsigned i=0;i<count;i+=4){assert(cursor[type]<96);u32* value=gpu.vectors[type][cursor[type]++];value[0]=32;memcpy(value+1,data+i,16);}
 dataWords+=count;
}
void GPUCMD_AddIncrementalWrites(unsigned reg,const u32* data,unsigned count){
 unsigned type=reg<0x2b0;assert(reg+(type?0x30:0)==GPUREG_VSH_FLOATUNIFORM_CONFIG && count==4 && data[0]<96);
 u32* value=gpu.vectors[type][data[0]];value[0]=24;memcpy(value+1,data+1,12);value[4]=0;dataWords+=3;
}
'''
code = prelude + 'namespace Reference {\n' + fixture + clean(root/'tools/3ds/tests/reference/citro3d_171/uniforms.c') + '\n}\n'
code += '#define SRR2_OLD3DS_NATIVE_BENCH\nnamespace Candidate {\n' + fixture + clean(root/'libs/pure3d/pddi/pica/nativeuniformsnapshot.h') + clean(root/'libs/pure3d/pddi/pica/nativeuniforms.c') + '\n}\n'
tracking=(root/'libs/pure3d/pddi/pica/trackeduniforms.hpp').read_text()
tracking=tracking[tracking.index('static inline void picaTrackUniforms'):tracking.index('static inline C3D_FVec*')]
code += 'namespace Candidate {\n'+tracking+'\n}\n'
code += r''' 
unsigned state=124829;unsigned randomValue(){state=state*1664525u+1013904223u;return state;}
int main(){
 float24Uniform_s constants[2][3]={};DVLE dvle[2]={{GPU_VERTEX_SHADER},{GPU_GEOMETRY_SHADER}};
 for(unsigned trial=0;trial<60000;++trial){
  Candidate::fast=trial%19!=0;Candidate::g_picaNativeTrackedFlush=trial%3!=0;
  const GPU_SHADER_TYPE type=GPU_SHADER_TYPE(trial%2);
  if(trial%11==0){
   const unsigned id=randomValue()%96;
   memset(Reference::gpu.vectors[type][id],0x35,sizeof(Reference::gpu.vectors[type][id]));
   memcpy(Candidate::gpu.vectors[type][id],Reference::gpu.vectors[type][id],sizeof(Reference::gpu.vectors[type][id]));
   Candidate::picaNativeUniformInvalidate(type,id,1);
   if(Candidate::g_picaNativeTrackedFlush)Candidate::picaTrackUniforms(type,id,1);
   Reference::C3D_FVUnifDirty[type][id]=Candidate::C3D_FVUnifDirty[type][id]=true;
  }
  if(trial%13==0){
   for(unsigned i=0;i<3;++i){constants[type][i].id=randomValue()%96;for(unsigned j=0;j<3;++j)constants[type][i].data[j]=randomValue();}
   shaderInstance_s shader={&dvle[type],u16(randomValue()),u16(randomValue()),{},constants[type],randomValue()%16,trial%3};
   for(unsigned i=0;i<4;++i)shader.intUniforms[i]=randomValue();
   Reference::C3Di_LoadShaderUniforms(&shader);Candidate::C3Di_LoadShaderUniforms(&shader);
  }
  if(trial%37==0){Reference::C3Di_ClearShaderUniforms(type);Candidate::C3Di_ClearShaderUniforms(type);}
  if(trial%47==0){
   memset(&Reference::gpu,0xcd,sizeof(Reference::gpu));memcpy(&Candidate::gpu,&Reference::gpu,sizeof(Reference::gpu));
   for(unsigned t=0;t<2;++t){Reference::C3Di_DirtyUniforms(GPU_SHADER_TYPE(t));Candidate::C3Di_DirtyUniforms(GPU_SHADER_TYPE(t));}
  }
  const unsigned count=trial%5==0?75:trial%7==0?96:randomValue()%6;
  const unsigned first=randomValue()%(97-count);
  if(Candidate::g_picaNativeTrackedFlush)Candidate::picaTrackUniforms(type,first,count);
  for(unsigned i=first;i<first+count;++i){
   Reference::C3D_FVUnifDirty[type][i]=Candidate::C3D_FVUnifDirty[type][i]=true;
   if(trial%4==0)for(unsigned j=0;j<4;++j)Reference::C3D_FVUnif[type][i].data[j]=Candidate::C3D_FVUnif[type][i].data[j]=randomValue();
  }
  if(trial%3==0){unsigned id=randomValue()%4;Reference::C3D_IVUnifDirty[type][id]=Candidate::C3D_IVUnifDirty[type][id]=true;Reference::C3D_IVUnif[type][id]=Candidate::C3D_IVUnif[type][id]=randomValue();}
  if(trial%5==0){Reference::C3D_BoolUnifsDirty[type]=Candidate::C3D_BoolUnifsDirty[type]=true;Reference::C3D_BoolUnifs[type]=Candidate::C3D_BoolUnifs[type]=randomValue();}
  for(unsigned t=0;t<2;++t){Reference::C3D_UpdateUniforms(GPU_SHADER_TYPE(t));Candidate::C3D_UpdateUniforms(GPU_SHADER_TYPE(t));}
  assert(!memcmp(&Reference::gpu,&Candidate::gpu,sizeof(Reference::gpu)));
  assert(!memcmp(Reference::C3D_FVUnifDirty,Candidate::C3D_FVUnifDirty,sizeof(Reference::C3D_FVUnifDirty)));
  assert(!memcmp(Reference::C3Di_FVUnifEverDirty,Candidate::C3Di_FVUnifEverDirty,sizeof(Reference::C3Di_FVUnifEverDirty)));
  assert(!memcmp(Reference::C3D_IVUnifDirty,Candidate::C3D_IVUnifDirty,sizeof(Reference::C3D_IVUnifDirty)));
  assert(!memcmp(Reference::C3D_BoolUnifsDirty,Candidate::C3D_BoolUnifsDirty,sizeof(Reference::C3D_BoolUnifsDirty)));
 }
 assert(Candidate::dataWords<Reference::dataWords);
 printf("PASS: 120000 tracked/scanned uniform updates match upstream register effects and dirty/restore state; data words %llu -> %llu. Not hardware timing.\n",Reference::dataWords,Candidate::dataWords);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-native-uniforms-') as directory:
    p=Path(directory);(p/'test.cpp').write_text(code)
    subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
