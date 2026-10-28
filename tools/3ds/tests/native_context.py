#!/usr/bin/env python3
"""Verify upstream state parity plus the required PICA fog-state render barrier."""
from pathlib import Path
import subprocess,tempfile,re,shutil,sys
root=Path(__file__).resolve().parents[3]
refs=root/'out/native-context-105/references'
reference=root/'tools/3ds/tests/reference/citro3d_171'
for name in ['effect.c','texture.c','texenv.c','attribs.c','buffers.c']:
    target=reference/name
    if not target.exists():shutil.copy2(refs/name,target)
base=reference/'base.c'
if not base.exists():shutil.copy2(root/'out/replay-submit-102/references/base.c',base)
def extract(text,signature):
    start=text.index(signature);end=text.index('{',start);level=1;end+=1
    while level:
        level+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]+'\n'
def clean(text):return '\n'.join(l for l in text.splitlines() if not l.startswith('#include')).replace('_Static_assert','static_assert')
internal=(root/'libs/pure3d/pddi/pica/citro3d_171/internal.h').read_text()
effect=internal[internal.index('typedef struct'):internal.index('typedef struct',internal.index('typedef struct')+1)]
flags=internal[internal.index('enum\n{'):internal.index('enum\n{',internal.index('enum\n{')+1)]
source=r'''
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <cassert>
#include <cstdio>
#include <thread>
using u16=uint16_t;using u32=uint32_t;using u64=uint64_t;using GPU_FOGMODE=unsigned;using GPU_GASMODE=unsigned;using GPU_CULLMODE=unsigned;using GPU_EARLYDEPTHFUNC=unsigned;using GPU_LOGICOP=unsigned;using GPU_GASDEPTHFUNC=unsigned;using GPU_SHADER_TYPE=unsigned;using C3D_LightEnv=void;
constexpr unsigned GPU_VERTEX_SHADER=0,GPU_GEOMETRY_SHADER=1,GPU_GAS=7,GPU_NO_FOG=0;
#define BIT(n) (1u<<(n))
#define GPU_MAKEGASDEPTHFUNC(n) ((0xAF02>>((int)(n)<<1))&3)
#define GPUCMD_HEADER(i,m,r) ((u32(i)<<31)|((u32(m)&15)<<16)|(u32(r)&1023))
struct C3D_AttrInfo{u32 flags[2];u64 permutation;int attrCount;};
struct C3D_BufCfg{u32 offset,flags[2];};struct C3D_BufInfo{u32 base_paddr;int bufCount;C3D_BufCfg buffers[12];};
struct C3D_TexEnv{u32 values[5];};struct C3D_TexCube{void* data[6];};
struct C3D_Tex{union{void* data;C3D_TexCube* cube;};unsigned fmt;u32 dim,param,border,lodParam;};
constexpr int C3D_FVUNIF_COUNT=96,C3D_IVUNIF_COUNT=4;
using C3D_IVec=u32;
struct float24Uniform_s{u32 id,data[3];};struct DVLE{GPU_SHADER_TYPE type;};
struct shaderInstance_s{DVLE* dvle;u16 boolUniforms,boolUniformMask;u32 intUniforms[4];float24Uniform_s* float24Uniforms;unsigned intUniformMask,numFloat24Uniforms;};
struct C3D_FVec{float x,y,z,w;};struct shaderProgram_s{void* geometryShader;};
struct Fog{u32 data[128];};struct FrameBuf{u32 marker;};
bool C3Di_TexIs2D(C3D_Tex* t){unsigned type=(t->param>>28)&7;return type!=1 && type!=4;}
'''+effect+flags+r'''
struct C3D_Context{
 u32 flags;shaderProgram_s* program;C3D_AttrInfo attrInfo;C3D_BufInfo bufInfo;C3D_Effect effect;void* lightEnv;
 u32 texConfig,texShadow;C3D_Tex* tex[3];C3D_TexEnv texEnv[6];u32 texEnvBuf,texEnvBufClr,fogClr;Fog* fogLut;
 FrameBuf fb;u32 viewport[5],scissor[3];unsigned fixedAttribDirty;C3D_FVec fixedAttribs[12];
};
'''
regs=Path('/opt/devkitpro/libctru/include/3ds/gpu/registers.h').read_text()
source+='\n'.join(l for l in regs.splitlines() if re.match(r'#define GPUREG_\w+\s+0x[0-9A-Fa-f]+',l))+'\n'
fixture=r'''
C3D_Context context;std::vector<u32> words;unsigned appends=0;
u32 storage[8192];u32* gpuCmdBuf=storage;unsigned gpuCmdBufOffset=0,gpuCmdBufSize=8192;
bool uniformFast=true;bool nativeBenchNativeUniforms(){return uniformFast;}
C3D_Context* C3Di_GetContext(){return &context;}
void C3D_FogGasMode(GPU_FOGMODE fog,GPU_GASMODE gas,bool flip){if(!(context.flags&C3DiF_Active))return;context.flags|=C3DiF_TexEnvBuf;context.texEnvBuf=(context.texEnvBuf&~0x100FFu)|(fog&7u)|((gas&1u)<<3)|(flip?BIT(16):0);}
void C3D_FogColor(u32 color){if(!(context.flags&C3DiF_Active))return;context.flags|=C3DiF_TexEnvBuf;context.fogClr=color;}

u32 osConvertVirtToPhys(const void* p){return uintptr_t(p);}
void GPUCMD_AddRawCommands(const u32* p,unsigned n){++appends;assert(gpuCmdBufOffset+n<=gpuCmdBufSize);memcpy(storage+gpuCmdBufOffset,p,n*4);gpuCmdBufOffset+=n;}
void GPUCMD_Add(u32 header,const u32* p,unsigned n){
 if(!n)n=1;
 while(n){unsigned count=n>256?256:n;++appends;assert(gpuCmdBufOffset+count+2<=gpuCmdBufSize);
 storage[gpuCmdBufOffset++]=p[0];storage[gpuCmdBufOffset++]=header|((count-1)<<20);
 memcpy(storage+gpuCmdBufOffset,p+1,(count-1)*4);gpuCmdBufOffset+=count-1;
 if(gpuCmdBufOffset&1)storage[gpuCmdBufOffset++]=0;
 n-=count;p+=count;if(header&BIT(31))header+=count;}
}
void GPUCMD_AddWrite(unsigned r,u32 v){GPUCMD_Add(GPUCMD_HEADER(0,15,r),&v,1);}
void GPUCMD_AddMaskedWrite(unsigned r,unsigned m,u32 v){GPUCMD_Add(GPUCMD_HEADER(0,m,r),&v,1);}
void GPUCMD_AddIncrementalWrites(unsigned r,const u32* p,unsigned n){GPUCMD_Add(GPUCMD_HEADER(1,15,r),p,n);}
void GPUCMD_AddWrites(unsigned r,const u32* p,unsigned n){GPUCMD_Add(GPUCMD_HEADER(0,15,r),p,n);}
void C3Di_FrameBufBind(FrameBuf* p){GPUCMD_AddWrite(0x310,p->marker);}
void shaderProgramConfigure(shaderProgram_s*,bool vertex,bool geometry){GPUCMD_AddWrite(0x311,vertex|unsigned(geometry)<<1);}
void C3Di_ProcTexUpdate(C3D_Context* c){GPUCMD_AddWrite(0x312,c->flags);c->flags&=~(C3DiF_ProcTex|C3DiF_ProcTexColorLut|C3DiF_ProcTexLutAll);}
void C3Di_GasUpdate(C3D_Context* c){GPUCMD_AddWrite(0x313,c->texEnvBuf);}
void C3Di_LightEnvUpdate(void*){GPUCMD_AddWrite(0x314,1);}
void C3D_ImmSendAttrib(float x,float y,float z,float w){float v[]={x,y,z,w};GPUCMD_AddIncrementalWrites(0x315,reinterpret_cast<u32*>(v),4);}
void C3D_UpdateUniforms(GPU_SHADER_TYPE type);
void __wrap_C3D_UpdateUniforms(GPU_SHADER_TYPE type){C3D_UpdateUniforms(type);}
'''
methods=''
for filename,signature in [('effect.c','void C3Di_EffectBind('),('texture.c','void C3Di_SetTex('),('texenv.c','void C3Di_TexEnvBind('),('attribs.c','void C3Di_AttrInfoBind('),('buffers.c','void C3Di_BufInfoBind(')]:
    methods+=extract((reference/filename).read_text(),signature)
update=extract(base.read_text(),'void C3Di_UpdateContext(')
commands=clean((root/'libs/pure3d/pddi/pica/nativecommands.h').read_text())
uniforms=clean((root/'libs/pure3d/pddi/pica/nativeuniforms.c').read_text())
uniform_snapshot=clean((root/'libs/pure3d/pddi/pica/nativeuniformsnapshot.h').read_text())
context_snapshot=clean((root/'libs/pure3d/pddi/pica/nativecontextsnapshot.h').read_text())
source+='#ifndef TEST_NO_BENCH\n#define SRR2_OLD3DS_NATIVE_BENCH\n#endif\n#define SRR2_OLD3DS_NATIVE_CONTEXT\n'
source+='namespace Reference {\n'+commands+fixture+uniform_snapshot+'\n'+context_snapshot+'\n'+uniforms+'\n'+methods+update+'picaNativeCommandBlock* g_picaNativeCommandBlock=nullptr;\n}\n'
candidate_fixture=fixture.replace('void GPUCMD_Add(u32 header,const u32* p,unsigned n){','void __real_GPUCMD_Add(u32 header,const u32* p,unsigned n){')
candidate_fixture='\nvoid __wrap_GPUCMD_Add(u32,const u32*,u32);\nvoid GPUCMD_Add(u32 h,const u32* p,unsigned n){__wrap_GPUCMD_Add(h,p,n); }\n'+candidate_fixture
source+='#undef PICA_NATIVE_COMMANDS_H\n#undef PICA_NATIVE_UNIFORM_SNAPSHOT_H\n#undef PICA_NATIVE_CONTEXT_SNAPSHOT_H\nnamespace Candidate {\n'+commands+candidate_fixture+uniform_snapshot+'\n'+context_snapshot+'\n'+uniforms+'\n'+methods+update.replace('C3Di_UpdateContext(','__real_C3Di_UpdateContext(')+clean((root/'libs/pure3d/pddi/pica/nativecontext.c').read_text())+'\n}\n'
source+=r'''
unsigned state=127843;unsigned randomValue(){state=state*1664525u+1013904223u;return state;}
bool sameStateWithFogBarrier(const std::vector<u32>& expected,const std::vector<u32>& actual){
 // Preserve every upstream word and require the extra flush immediately before
 // the fog write. A flush before shader setup does not satisfy this contract.
 size_t fog=expected.size();
 for(size_t at=0;at+1<expected.size();){
  const u32 header=expected[at+1];
  if((header&1023)==GPUREG_TEXENV_UPDATE_BUFFER){fog=at;break;}
  at+=(1+((header>>20)&2047)+2)&~size_t(1);
 }
 if(fog==expected.size())return actual==expected;
 if(actual.size()!=expected.size()+2 || actual[fog]!=1 ||
    actual[fog+1]!=GPUCMD_HEADER(0,15,GPUREG_FRAMEBUFFER_FLUSH))return false;
 auto withoutBarrier=actual;withoutBarrier.erase(withoutBarrier.begin()+fog,withoutBarrier.begin()+fog+2);
 return withoutBarrier==expected;
}
int main(){
#ifndef SRR2_OLD3DS_NATIVE_BENCH
 assert(Candidate::g_picaNativeContextEnabled);
#endif
 unsigned long oldAppends=0,newAppends=0;unsigned fast=0,ownedSnapshots=0;
 C3D_Tex textures[3]={};C3D_TexCube cubes[3]={};shaderProgram_s program={},reflection={},contact={},foreignProgram={};Fog fog={};
 Candidate::picaNativeRegisterPrograms(&program,nullptr);
 Candidate::picaNativeRegisterReflectionProgram(&reflection);
 Candidate::picaNativeRegisterContactProgram(&contact);
 Candidate::context={};Candidate::context.flags=C3DiF_Active;
 Candidate::context.tex[0]=&textures[0];Candidate::context.tex[2]=&textures[2];
 Candidate::picaNativeBindTexture1(&textures[1]);
 assert(Candidate::context.tex[1]==&textures[1] && (Candidate::context.flags&C3DiF_Tex(1)));
 Candidate::context.flags=C3DiF_Active;Candidate::picaNativeBindTexture1(nullptr);
 assert(!Candidate::context.tex[1] && (Candidate::context.flags&C3DiF_Tex(1)));
 assert(Candidate::context.tex[0]==&textures[0] && Candidate::context.tex[2]==&textures[2]);
 textures[1].param=1u<<28;Candidate::context.flags=C3DiF_Active;
 Candidate::picaNativeBindTexture1(&textures[1]);
 assert(!Candidate::context.tex[1] && Candidate::context.flags==C3DiF_Active);
 textures[1].param=0;Candidate::context.flags=0;Candidate::picaNativeBindTexture1(&textures[1]);
 assert(!Candidate::context.tex[1] && !Candidate::context.flags);
 for(unsigned trial=0;trial<40000;++trial){
  C3D_Context initial={};initial.flags=C3DiF_Active;
  const unsigned common=C3DiF_DrawUsed|C3DiF_AttrInfo|C3DiF_BufInfo|C3DiF_Effect|C3DiF_TexAll|C3DiF_TexStatus|C3DiF_TexEnvAll|C3DiF_Viewport|C3DiF_Scissor|C3DiF_TexEnvBuf|C3DiF_LightEnv|C3DiF_FogLut|C3DiF_Gas|C3DiF_GasLut;
  initial.flags|=randomValue()&common;initial.program=&program;program.geometryShader=trial%41==0?&program:nullptr;
  reflection.geometryShader=contact.geometryShader=program.geometryShader;
  if(trial%3==1)initial.program=&reflection;else if(trial%3==2)initial.program=&contact;
  initial.attrInfo.attrCount=1+randomValue()%12;initial.attrInfo.flags[0]=randomValue();initial.attrInfo.flags[1]=randomValue();initial.attrInfo.permutation=(u64(randomValue())<<32)|randomValue();
  initial.bufInfo.base_paddr=0x18000000;initial.bufInfo.bufCount=1+randomValue()%12;
  for(auto& b:initial.bufInfo.buffers){b.offset=randomValue();b.flags[0]=randomValue();b.flags[1]=randomValue();}
  u32* raw=reinterpret_cast<u32*>(&initial.effect);for(unsigned i=0;i<sizeof(initial.effect)/4;++i)raw[i]=randomValue();
  initial.effect.zBuffer=trial%2;initial.effect.earlyDepth=trial%3==0;
  for(unsigned i=0;i<3;++i){auto& t=textures[i];t.border=randomValue();t.dim=randomValue();t.param=randomValue();t.lodParam=randomValue();t.fmt=randomValue()%16;t.cube=&cubes[i];
   if(C3Di_TexIs2D(&t))t.data=reinterpret_cast<void*>(uintptr_t(randomValue()));
   else for(unsigned j=0;j<6;++j)cubes[i].data[j]=reinterpret_cast<void*>(uintptr_t(randomValue()));
   initial.tex[i]=randomValue()%4?&t:nullptr;}
  for(auto& e:initial.texEnv)for(auto& v:e.values)v=randomValue();
  initial.texConfig=randomValue();initial.texShadow=randomValue();initial.texEnvBuf=(trial%47==0?GPU_GAS:trial%7)|(trial%2?BIT(16):0);initial.texEnvBufClr=randomValue();initial.fogClr=randomValue();initial.fogLut=&fog;
  for(auto& v:initial.viewport)v=randomValue();for(auto& v:initial.scissor)v=randomValue();
  // Rare context state must forward unchanged, including fixed attributes/restore.
  if(trial%5==0)initial.flags|=C3DiF_FrameBuf|C3DiF_Viewport|C3DiF_Scissor|C3DiF_Program|C3DiF_TexEnvBuf|C3DiF_FogLut;
  if(trial%13==0)initial.lightEnv=&fog;
  if(trial%17==0){initial.fixedAttribDirty=randomValue()&4095;initial.flags|=C3DiF_LightEnv;}
  if(trial%19==0)initial.flags|=C3DiF_ProcTex;
  if(trial%37==0){initial.flags=C3DiF_Active|common;initial.texEnvBuf=GPU_NO_FOG;initial.lightEnv=nullptr;initial.fixedAttribDirty=0;program.geometryShader=nullptr;}
  if(trial%53==0)initial.program=&foreignProgram;
  Reference::context=Candidate::context=initial;Reference::words.clear();Candidate::words.clear();Reference::appends=Candidate::appends=0;
  Reference::gpuCmdBufOffset=Candidate::gpuCmdBufOffset=0;
  Reference::gpuCmdBufSize=Candidate::gpuCmdBufSize=trial%23==0?2047:8192;
  Reference::uniformFast=Candidate::uniformFast=trial%29!=0;
  float24Uniform_s constants[2][9]={};
  for(unsigned t=0;t<2;++t){
   Reference::C3Di_ShaderFVecData[t].dirty=Candidate::C3Di_ShaderFVecData[t].dirty=trial%7==0;
   unsigned count=trial%31==0?9:trial%7==0?3:0;
   Reference::C3Di_ShaderFVecData[t].count=Candidate::C3Di_ShaderFVecData[t].count=count;
   Reference::C3Di_ShaderFVecData[t].data=Candidate::C3Di_ShaderFVecData[t].data=constants[t];
   for(unsigned i=0;i<9;++i){constants[t][i].id=randomValue()%96;for(auto& v:constants[t][i].data)v=randomValue();}
   for(unsigned i=0;i<96;++i){
    bool dirty=trial%37==0 || randomValue()%4==0;
    Reference::C3D_FVUnifDirty[t][i]=Candidate::C3D_FVUnifDirty[t][i]=dirty;
    if(dirty){Reference::g_picaNativeUniformMask[t][i/32]|=BIT(i%32);Candidate::g_picaNativeUniformMask[t][i/32]|=BIT(i%32);}
    if(trial%3!=0){u32 v[4];for(auto& x:v)x=randomValue();memcpy(&Reference::C3D_FVUnif[t][i],v,16);memcpy(&Candidate::C3D_FVUnif[t][i],v,16);}
   }
   for(unsigned i=0;i<4;++i){Reference::C3D_IVUnif[t][i]=Candidate::C3D_IVUnif[t][i]=randomValue();Reference::C3D_IVUnifDirty[t][i]=Candidate::C3D_IVUnifDirty[t][i]=randomValue()%2;}
   Reference::C3D_BoolUnifs[t]=Candidate::C3D_BoolUnifs[t]=randomValue();Reference::C3D_BoolUnifsDirty[t]=Candidate::C3D_BoolUnifsDirty[t]=true;
   if(trial%43==0){Reference::C3Di_DirtyUniforms(t);Candidate::C3Di_DirtyUniforms(t);}
  }
  Candidate::g_picaNativeContextEnabled=trial%11!=0;
  Reference::C3Di_UpdateContext();
#ifdef TEST_OWNED_SNAPSHOT
  if(Candidate::picaNativeCanCaptureContext() && Candidate::gpuCmdBufSize>=2048) {
   ++ownedSnapshots;Candidate::picaNativeContextSnapshot captured;
   Candidate::picaNativeCaptureContext(&captured);
   // Force a relocated, owned record and overwrite all mutable source payload.
   // The original context is restored only after compilation, for comparison.
   auto relocated=captured;
   memset(&captured,0x97,sizeof(captured));
   Candidate::picaNativeCommandBlock output={Candidate::storage,0};
   const auto compile=[&]{Candidate::picaNativeCompileContext(&output,&relocated,Candidate::picaNativeSharedUniformCache());};
#ifdef TEST_CONCURRENT_SNAPSHOT
   std::thread compiler(compile);
#endif
   auto contextAfterCapture=Candidate::context;
   Candidate::context={};
   C3D_Tex savedTextures[3];C3D_TexCube savedCubes[3];
   memcpy(savedTextures,textures,sizeof(textures));memcpy(savedCubes,cubes,sizeof(cubes));
   memset(textures,0x86,sizeof(textures));memset(cubes,0x75,sizeof(cubes));
   C3D_FVec savedVectors[2][96];memcpy(savedVectors,Candidate::C3D_FVUnif,sizeof(savedVectors));
   memset(Candidate::C3D_FVUnif,0x64,sizeof(Candidate::C3D_FVUnif));
   float24Uniform_s savedConstants[2][9];memcpy(savedConstants,constants,sizeof(constants));
   memset(constants,0x53,sizeof(constants));
#ifdef TEST_CONCURRENT_SNAPSHOT
   compiler.join();
#else
   compile();
#endif
   Candidate::gpuCmdBufOffset=output.count;
   Candidate::context=contextAfterCapture;
   memcpy(textures,savedTextures,sizeof(textures));memcpy(cubes,savedCubes,sizeof(cubes));
   memcpy(Candidate::C3D_FVUnif,savedVectors,sizeof(savedVectors));memcpy(constants,savedConstants,sizeof(constants));
  } else Candidate::picaNativeUpdateContext();
#elif defined(SRR2_OLD3DS_SUBMIT_PROFILE)
  Candidate::picaNativeUpdateContext();
#else
  Candidate::__wrap_C3Di_UpdateContext();
#endif
  Reference::words.assign(Reference::storage,Reference::storage+Reference::gpuCmdBufOffset);
  Candidate::words.assign(Candidate::storage,Candidate::storage+Candidate::gpuCmdBufOffset);
  assert(sameStateWithFogBarrier(Reference::words,Candidate::words));
  assert(!memcmp(Reference::C3D_FVUnifDirty,Candidate::C3D_FVUnifDirty,sizeof(Reference::C3D_FVUnifDirty)));
  assert(!memcmp(Reference::C3Di_FVUnifEverDirty,Candidate::C3Di_FVUnifEverDirty,sizeof(Reference::C3Di_FVUnifEverDirty)));
  assert(!Candidate::g_picaNativeCommandBlock);
  assert(Candidate::gpuCmdBufOffset<=Candidate::gpuCmdBufSize);
  assert(!memcmp(&Reference::context,&Candidate::context,sizeof(initial)));
  assert(!Candidate::g_picaNativeTrackedFlush);
  oldAppends+=Reference::appends;newAppends+=Candidate::appends;fast+=Candidate::g_picaNativeContextEnabled && Reference::appends>Candidate::appends;
 }
#ifdef TEST_OWNED_SNAPSHOT
 assert(ownedSnapshots>9000);
 printf("Owned snapshot cases: %u; relocated records survive overwritten live context, textures, cubes, constants and uniform arrays.\n",ownedSnapshots);
#else
 (void)ownedSnapshots;
#endif
 assert(fast>9000 && newAppends<oldAppends);
 printf("PASS: 40000 upstream state comparisons with one required flush before each fog-state write; %u common-path reductions, appends %lu -> %lu. Not hardware timing.\n",fast,oldAppends,newAppends);
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
 Candidate::context={};Candidate::context.flags=C3DiF_Active;Candidate::context.program=&reflection;
 reflection.geometryShader=nullptr;Candidate::g_picaNativeContextEnabled=true;Candidate::uniformFast=true;
 memset(Candidate::C3Di_ShaderFVecData,0,sizeof(Candidate::C3Di_ShaderFVecData));
 memset(Candidate::C3D_FVUnifDirty,0,sizeof(Candidate::C3D_FVUnifDirty));
 memset(Candidate::C3D_IVUnifDirty,0,sizeof(Candidate::C3D_IVUnifDirty));
 memset(Candidate::C3D_BoolUnifsDirty,0,sizeof(Candidate::C3D_BoolUnifsDirty));
 memset(Candidate::g_picaNativeUniformMask,0,sizeof(Candidate::g_picaNativeUniformMask));
 Candidate::gpuCmdBufSize=8192;
 Candidate::picaNativeConfigureReflectionOutputs(0x1f1f0d0c,0x103,0x0f0e0d0c,0x303);
 unsigned ordinaryCount=0,reflectionCount=0,sparse=1;
 Candidate::picaNativeReflectionOutputStats(&ordinaryCount,&reflectionCount,&sparse);assert(sparse);
 Candidate::picaNativeEnableSparseReflectionOutputs(true);
 const auto check=[&](bool reflected,bool expectWrites){
  Candidate::picaNativeSetReflectionOutput(reflected);Candidate::gpuCmdBufOffset=0;
  Candidate::picaNativeUpdateContext();
  assert(Candidate::gpuCmdBufOffset==(expectWrites?4:0));
  if(expectWrites){assert(Candidate::storage[0]==(reflected?0x0f0e0d0cu:0x1f1f0d0cu));
   assert(Candidate::storage[1]==GPUCMD_HEADER(0,15,GPUREG_SH_OUTMAP_O2));
   assert(Candidate::storage[2]==(reflected?0x303u:0x103u));
   assert(Candidate::storage[3]==GPUCMD_HEADER(0,15,GPUREG_SH_OUTATTR_CLOCK));}
 };
 check(false,true);check(false,false);check(true,true);check(true,false);
 Candidate::picaNativeInvalidateReflectionOutputs();check(true,true);
 Candidate::context.program=&contact;check(true,false);
 Candidate::context.program=&reflection;check(true,true);
 Candidate::context.flags|=C3DiF_Program;
 Candidate::gpuCmdBufOffset=0;Candidate::picaNativeUpdateContext();
 assert(Candidate::storage[Candidate::gpuCmdBufOffset-4]==0x0f0e0d0c);
 assert(Candidate::storage[Candidate::gpuCmdBufOffset-2]==0x303);
 Candidate::picaNativeEnableSparseReflectionOutputs(false);
 Candidate::picaNativeSetReflectionOutput(false);Candidate::gpuCmdBufOffset=0;
 Candidate::picaNativeUpdateContext();assert(Candidate::storage[0]==0x0f0e0d0c);
 Candidate::picaNativeEnableSparseReflectionOutputs(true);check(false,true);
 assert(!Candidate::picaNativeCanCaptureContext());
 printf("PASS: SDK-derived UV0/UV1 routing, cache hits, foreign programs, SDK reconfiguration and diagnostic rollback.\n");
 Candidate::picaNativeConfigureReflectionEntries(&reflection,0,73,&program,0,91);
 const auto entry=[&](bool reflected,unsigned address,unsigned count,bool full){
  Candidate::picaNativeSetReflectionOutput(reflected);Candidate::gpuCmdBufOffset=0;
  Candidate::context.flags=C3DiF_Active;Candidate::picaNativeUpdateContext();
  assert(Candidate::gpuCmdBufOffset==count);
  if(count){assert(Candidate::storage[0]==(0x7fff0000u|address));
   assert(Candidate::storage[1]==GPUCMD_HEADER(0,15,GPUREG_VSH_ENTRYPOINT));}
  if(count==6){assert(Candidate::storage[2]==(full?0x0f0e0d0cu:0x1f1f0d0cu));
   assert(Candidate::storage[3]==GPUCMD_HEADER(0,15,GPUREG_SH_OUTMAP_O2));
   assert(Candidate::storage[4]==(full?0x303u:0x103u));
   assert(Candidate::storage[5]==GPUCMD_HEADER(0,15,GPUREG_SH_OUTATTR_CLOCK));}
 };
 entry(false,0,6,false);entry(false,0,0,false);entry(true,73,6,true);entry(true,73,0,true);
 Candidate::context.program=&contact;Candidate::gpuCmdBufOffset=0;Candidate::picaNativeUpdateContext();
 Candidate::context.program=&reflection;entry(false,0,6,false);
 Candidate::picaNativeRegisterReflectionProgram(&program);Candidate::context.program=&program;
 entry(true,91,6,true);
 Candidate::picaNativeRegisterReflectionProgram(&reflection);Candidate::context.program=&reflection;
 entry(true,73,6,true);
 Candidate::picaNativeEnableSparseReflectionOutputs(false);
 entry(false,0,6,true);entry(true,73,2,true);entry(false,0,2,true);
 Candidate::picaNativeEnableSparseReflectionOutputs(true);entry(false,0,6,false);
 unsigned entryOrdinary,entryReflected,entriesEnabled;
 Candidate::picaNativeReflectionEntryStats(&entryOrdinary,&entryReflected,&entriesEnabled);assert(entriesEnabled);
 Candidate::context.flags|=C3DiF_Program;Candidate::gpuCmdBufOffset=0;Candidate::picaNativeUpdateContext();
 unsigned afterOrdinary,afterReflected;
 Candidate::picaNativeReflectionEntryStats(&afterOrdinary,&afterReflected,&entriesEnabled);
 assert(afterOrdinary==entryOrdinary && afterReflected==entryReflected);
 Candidate::picaNativeSetReflectionOutput(true);Candidate::context.flags|=C3DiF_Program;
 Candidate::gpuCmdBufOffset=0;Candidate::picaNativeUpdateContext();
 assert(Candidate::storage[Candidate::gpuCmdBufOffset-6]==(0x7fff0000u|73));
 Candidate::picaNativeInvalidateReflectionOutputs();entry(false,0,6,false);
 // A temporary geometry stage on the same owned object must not leave a clean
 // routing cache when the ordinary vertex-only program returns.
 reflection.geometryShader=reinterpret_cast<shaderInstance_s*>(1);
 Candidate::context.flags=C3DiF_Active;Candidate::gpuCmdBufOffset=0;
 Candidate::picaNativeUpdateContext();
 reflection.geometryShader=nullptr;
 entry(false,0,6,false);
 // Retain a pending material transition when the SDK fallback has no room for
 // the entry/map block. The next ordinary-sized buffer must apply it.
 Candidate::picaNativeSetReflectionOutput(true);Candidate::gpuCmdBufSize=4;
 Candidate::context.flags=C3DiF_Active;Candidate::gpuCmdBufOffset=0;
 Candidate::picaNativeUpdateContext();
 Candidate::gpuCmdBufSize=8192;entry(true,73,6,true);
 Candidate::picaNativeConfigureReflectionEntries(nullptr,0,0,nullptr,0,0);
 Candidate::picaNativeReflectionEntryStats(&afterOrdinary,&afterReflected,&entriesEnabled);assert(!entriesEnabled);
 printf("PASS: shared-code mono/stereo entries, independent UV routing, cache hits, SDK default restoration and explicit invalidation.\n");
#endif
}
'''
if '--emit-source' in sys.argv:
    Path(sys.argv[sys.argv.index('--emit-source')+1]).write_text(source);sys.exit(0)
with tempfile.TemporaryDirectory(prefix='srr-native-context-') as directory:
    p=Path(directory);(p/'test.cpp').write_text(source)
    variants=([['-DTEST_OWNED_SNAPSHOT','-DTEST_CONCURRENT_SNAPSHOT']] if '--tsan' in sys.argv else [['-DSRR2_OLD3DS_SUBMIT_PROFILE'],[],['-DTEST_NO_BENCH'],['-DTEST_OWNED_SNAPSHOT'],['-DSRR2_OLD3DS_VISUAL_UPGRADE','-DTEST_OWNED_SNAPSHOT']])
    for flags in variants:
        subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-sizeof-array-div','-pthread','-fsanitize='+('thread' if '--tsan' in sys.argv else 'address,undefined'),*flags,str(p/'test.cpp'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)
