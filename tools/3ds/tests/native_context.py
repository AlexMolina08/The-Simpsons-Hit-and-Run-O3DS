#!/usr/bin/env python3
"""Compare common native context packets with the upstream 1.7.1 command stream."""
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
using u16=uint16_t;using u32=uint32_t;using u64=uint64_t;using GPU_CULLMODE=unsigned;using GPU_EARLYDEPTHFUNC=unsigned;using GPU_LOGICOP=unsigned;using GPU_GASDEPTHFUNC=unsigned;using GPU_SHADER_TYPE=unsigned;using C3D_LightEnv=void;
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
source+='#undef PICA_NATIVE_COMMANDS_H\n#undef PICA_NATIVE_UNIFORM_SNAPSHOT_H\n#undef PICA_NATIVE_CONTEXT_SNAPSHOT_H\nnamespace Candidate {\n'+commands+fixture+uniform_snapshot+'\n'+context_snapshot+'\n'+uniforms+'\n'+methods+update.replace('C3Di_UpdateContext(','__real_C3Di_UpdateContext(')+clean((root/'libs/pure3d/pddi/pica/nativecontext.c').read_text())+'\n}\n'
source+=r'''
unsigned state=127843;unsigned randomValue(){state=state*1664525u+1013904223u;return state;}
int main(){
#ifndef SRR2_OLD3DS_NATIVE_BENCH
 assert(Candidate::g_picaNativeContextEnabled);
#endif
 unsigned long oldAppends=0,newAppends=0;unsigned fast=0,ownedSnapshots=0;
 C3D_Tex textures[3]={};C3D_TexCube cubes[3]={};shaderProgram_s program={},foreignProgram={};Fog fog={};
 Candidate::picaNativeRegisterPrograms(&program,nullptr);
 for(unsigned trial=0;trial<40000;++trial){
  C3D_Context initial={};initial.flags=C3DiF_Active;
  const unsigned common=C3DiF_DrawUsed|C3DiF_AttrInfo|C3DiF_BufInfo|C3DiF_Effect|C3DiF_TexAll|C3DiF_TexStatus|C3DiF_TexEnvAll|C3DiF_Viewport|C3DiF_Scissor|C3DiF_TexEnvBuf|C3DiF_LightEnv|C3DiF_FogLut|C3DiF_Gas|C3DiF_GasLut;
  initial.flags|=randomValue()&common;initial.program=&program;program.geometryShader=trial%41==0?&program:nullptr;
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
  assert(Reference::words==Candidate::words);
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
 printf("PASS: 40000 upstream context command/state comparisons; %u common-path reductions, appends %lu -> %lu. Rare state forwards unchanged. Not hardware timing.\n",fast,oldAppends,newAppends);
}
'''
if '--emit-source' in sys.argv:
    Path(sys.argv[sys.argv.index('--emit-source')+1]).write_text(source);sys.exit(0)
with tempfile.TemporaryDirectory(prefix='srr-native-context-') as directory:
    p=Path(directory);(p/'test.cpp').write_text(source)
    variants=([['-DTEST_OWNED_SNAPSHOT','-DTEST_CONCURRENT_SNAPSHOT']] if '--tsan' in sys.argv else [['-DSRR2_OLD3DS_SUBMIT_PROFILE'],[],['-DTEST_NO_BENCH'],['-DTEST_OWNED_SNAPSHOT']])
    for flags in variants:
        subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-sizeof-array-div','-pthread','-fsanitize='+('thread' if '--tsan' in sys.argv else 'address,undefined'),*flags,str(p/'test.cpp'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)
