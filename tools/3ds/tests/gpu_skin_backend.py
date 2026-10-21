#!/usr/bin/env python3
"""Compile production eligibility, upload, mutation, and retirement code with GPU spies."""
from pathlib import Path
import subprocess
import tempfile,sys
extra_budget="--extra-budget" in sys.argv
borrow="--borrow" in sys.argv or extra_budget
packed="--packed" in sys.argv or borrow
ROOT=Path(__file__).resolve().parents[3]
source=(ROOT/'libs/pure3d/pddi/pica/context.cpp').read_text()
def extract(signature):
    a=source.index(signature);b=source.index('{',a);depth=1;e=b+1
    while depth:
        depth+=(source[e]=='{')-(source[e]=='}');e+=1
    return source[a:e]
methods='\n'.join(extract(s) for s in ['bool picaContext::BindSkinProgram(bool enabled)',
 'void picaPrimBuffer::PrepareGpuSkin(', 'void picaPrimBuffer::MutateGpuSkin()', 'bool picaPrimBuffer::TryGpuSkin(', 'bool picaPrimBuffer::SubmitGpuSkin('])
methods += '\n#ifdef SRR2_OLD3DS_GPU_SKIN_BATCH\n'+'\n'.join(extract(s) for s in ['void picaContext::BeginSkinBatch()', 'void picaContext::EndSkinBatch()', 'void picaContext::SkinBatchBarrier()'])+'\n#endif\n'
methods += '\n#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK\n'+extract('void picaContext::SetSkinLayout(')+'\n#endif\n'
prelude=r'''
#include <cassert>
#define PDDIASSERT(x) assert(x)
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <pddi/pica/skinning.hpp>
#include <pddi/pica/skineyecache.hpp>
static unsigned freeSpace=8*1024*1024, flushes, draws, materialCalls, frees;
static bool failAllocation, programSkin;
static std::set<void*> allocations;
void* linearAlloc(unsigned n) {if(failAllocation)return NULL;void* p=malloc(n);allocations.insert(p);return p;}
void linearFree(void* p) {assert(allocations.erase(p)==1);free(p);++frees;}
unsigned linearSpaceFree(){return freeSpace;}
u64 svcGetSystemTick(){static u64 ticks=0;return ++ticks;}
float C3D_GetDrawingTime(){return 0;} float C3D_GetProcessingTime(){return 0;}
void GSPGPU_FlushDataCache(const void*,unsigned){++flushes;}
struct Vec {float data[4];float& operator[](unsigned i){return data[i];}const float& operator[](unsigned i)const{return data[i];}};
struct pddiMatrix {Vec rows[4];const Vec& Row4(unsigned i)const{return rows[i];}};
struct pddiShader{};
struct C3D_FVec{float x,y,z,w;};static C3D_FVec uniforms[96];
const float PICA_STEREO_CONVERGENCE=7;
const unsigned VSH_FVEC_u_eye=25, VSH_FVEC_u_eye_quant=95;
void C3D_FVUnifSet(int,unsigned i,float x,float y,float z,float w){assert(i==25 || i==95);if(i==95)assert(x==1.f/8192&&y==1.f/32768&&z==1.f/4096&&w==1);uniforms[i]={x,y,z,w};}
C3D_FVec* C3D_FVUnifWritePtr(int,unsigned first,unsigned count){assert(first==VSH_BONE_BASE && count<=3*picaSkin::MaxBones);return uniforms+first;}
void GPUCMD_GetBuffer(u32** p,u32* s,u32* o){*p=NULL;*s=10000;*o=100;}
void C3D_BindProgram(int* p){programSkin=*p==2;}
static int attributeLayout;void C3D_SetAttrInfo(int* p){attributeLayout=*p;}
const unsigned PDDI_V_NORMAL=16;
const int GPU_VERTEX_SHADER=0,VSH_FVEC_u_bones=VSH_BONE_BASE;
bool g_picaInFrame=true;
struct {bool bufInfoSet;} g_picaState;
bool PrimIsDrawable(int p){return p==1;}
struct picaPrimBuffer;
struct EyeStack {pddiMatrix matrix={{{{1,0,0,0}},{{0,1,0,0}},{{0,0,1,0}},{{0,0,0,1}}}};const pddiMatrix* Top(){return &matrix;}};
const int PDDI_MATRIX_MODELVIEW=0;
struct picaContext {
 void FlushPreparedExecution(bool){}bool PreparedBatching(){
 return false;
 }
 bool QueuePreparedProgram(int*,int*,bool,unsigned,float){return false;}bool QueuePreparedLayout(int*,bool){return false;}
 picaSkin::EyeCache skinEyeCache;
 EyeStack stack;struct {EyeStack* matrixStack[1];} state={{&stack}};
 bool bottomPass=false;bool DrawWheelSkin(pddiShader*,picaPrimBuffer*,const pddiMatrix*);
 bool skinProgramBound=false;void* skinBinary=(void*)1;int skinProgram=2,shaderProgram=1,skinAttrInfo=2,attrInfo=1;
 unsigned invalidations=0;
 void InvalidateUniformCache(){++invalidations;}
 bool skinRigidLayout=false;int skinRigidAttrInfo=3;void SetSkinLayout(bool);
 unsigned skinBatchDepth=0;void BeginSkinBatch();void EndSkinBatch();void SkinBatchBarrier();
 bool BindSkinProgram(bool);
 void DrawPrimBuffer(pddiShader*,picaPrimBuffer*);
};
struct picaPrimBuffer {
 bool wheelSkinBuffer=false;
 picaContext* context;bool transient=false;unsigned char* buffer;unsigned allocated=2;
 int primType=1;unsigned vertexType=16,mem=80;bool validIndices=true;
 bool IndicesInRange(){return validIndices;}
 picaSkinAllocation* skinAllocation=NULL;picaSkin::Reason skinReason=picaSkin::NotSelected;
 unsigned skinBoneCount=0;bool skinDrawing=false;bool skinRigid=false;
 void PrepareGpuSkin(const void*,unsigned,unsigned,bool,bool pedestrian=false,bool named=false);void MutateGpuSkin();
 bool TryGpuSkin(pddiShader*,const pddiMatrix* const*,unsigned);bool SubmitGpuSkin(pddiShader*);
};
void picaContext::DrawPrimBuffer(pddiShader*,picaPrimBuffer* p){assert(programSkin && p->skinDrawing);
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
 assert(attributeLayout==(p->skinRigid?3:2));
#endif
++draws;++materialCalls;}
'''
tests=r'''
int main(){
 picaContext context;pddiShader material;unsigned char buffer[72]={};picaSkin::Source src[2]={};
 for(auto& v:src){v.weights[0]=1;v.normal[2]=1;}
 pddiMatrix matrices[picaSkin::MaxBones]={};const pddiMatrix* palette[picaSkin::MaxBones];
 for(unsigned i=0;i<picaSkin::MaxBones;++i){palette[i]=&matrices[i];for(unsigned j=0;j<4;++j)matrices[i].rows[j][j]=1;matrices[i].rows[3][0]=float(i);}
 auto make=[&](){picaPrimBuffer p;p.context=&context;p.buffer=buffer;return p;};
 picaPrimBuffer unsupported=make();unsupported.PrepareGpuSkin(src,2,picaSkin::MaxBones+1,true);assert(unsupported.skinReason==picaSkin::Palette);
 unsupported=make();unsupported.validIndices=false;unsupported.PrepareGpuSkin(src,2,2,true);assert(unsupported.skinReason==picaSkin::Topology);
 unsupported=make();src[0].indices[0]=23;unsupported.PrepareGpuSkin(src,2,23,true);assert(unsupported.skinReason==picaSkin::SourceInvalid);src[0].indices[0]=0;
 failAllocation=true;unsupported=make();unsupported.PrepareGpuSkin(src,2,2,true);
#ifdef SRR2_OLD3DS_GPU_SKIN
 assert(!unsupported.skinAllocation && unsupported.skinReason!=picaSkin::Ready);
#endif
 failAllocation=false;
 picaPrimBuffer p=make();p.PrepareGpuSkin(src,2,picaSkin::MaxBones,true);assert(p.skinReason==picaSkin::Ready);
 unsigned initialFlushes=flushes;
 for(unsigned pose=0;pose<10;++pose){
  matrices[picaSkin::MaxBones-1].rows[3][0]=float(pose);g_picaState.bufInfoSet=true;
  bool used=p.TryGpuSkin(&material,palette,picaSkin::MaxBones);
#ifdef SRR2_OLD3DS_GPU_SKIN
  assert(used && !programSkin && !g_picaState.bufInfoSet && !p.skinDrawing);
  assert(uniforms[VSH_BONE_BASE+3*(picaSkin::MaxBones-1)].w==float(pose));
#ifdef SRR2_OLD3DS_GPU_SKIN_COMPACT
  assert(!context.skinProgramBound && uniforms[25].x==0 && uniforms[25].y==7);
#endif
#else
  assert(!used && !draws);
#endif
 }
 assert(flushes==initialFlushes);
 matrices[0].rows[0][0]=std::numeric_limits<float>::infinity();
 assert(!p.TryGpuSkin(&material,palette,picaSkin::MaxBones));matrices[0].rows[0][0]=1;
 g_picaInFrame=false;assert(!p.TryGpuSkin(&material,palette,picaSkin::MaxBones));g_picaInFrame=true;
#ifdef SRR2_OLD3DS_GPU_SKIN_BATCH
 context.BeginSkinBatch();unsigned beforeSwitch=g_picaSkinMetrics.switches;
 assert(p.TryGpuSkin(&material,palette,picaSkin::MaxBones));
 assert(p.TryGpuSkin(&material,palette,picaSkin::MaxBones));
 assert(programSkin&&context.skinProgramBound&&g_picaSkinMetrics.switches==beforeSwitch+1);
 picaPrimBuffer fallback=make();assert(!fallback.TryGpuSkin(&material,palette,0));assert(!programSkin);
 assert(p.TryGpuSkin(&material,palette,picaSkin::MaxBones));
 matrices[0].rows[0][0]=std::numeric_limits<float>::infinity();
 assert(!p.TryGpuSkin(&material,palette,picaSkin::MaxBones));assert(!programSkin);matrices[0].rows[0][0]=1;
 context.BeginSkinBatch();assert(p.TryGpuSkin(&material,palette,picaSkin::MaxBones));context.EndSkinBatch();
 assert(!programSkin&&context.skinBatchDepth==1);
 assert(p.TryGpuSkin(&material,palette,picaSkin::MaxBones));context.EndSkinBatch();
 assert(!programSkin&&!context.skinProgramBound&&!context.skinBatchDepth);
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
 assert(p.skinRigid&&p.skinAllocation->bytes==32+2*picaSkin::PackedStride(true));
 src[0].weights[0]=0.5f;src[0].weights[1]=0.25f;src[0].weights[2]=0.125f;
 picaPrimBuffer weighted=make();weighted.PrepareGpuSkin(src,2,picaSkin::MaxBones,true);
 assert(!weighted.skinRigid&&weighted.skinAllocation->bytes==32+2*picaSkin::PackedStride(false));
 context.BeginSkinBatch();
 assert(p.TryGpuSkin(&material,palette,picaSkin::MaxBones)&&attributeLayout==3);
 g_picaState.bufInfoSet=true;
 assert(weighted.TryGpuSkin(&material,palette,picaSkin::MaxBones)&&attributeLayout==2&&!g_picaState.bufInfoSet);
 g_picaState.bufInfoSet=true;
 assert(p.TryGpuSkin(&material,palette,picaSkin::MaxBones)&&attributeLayout==3&&!g_picaState.bufInfoSet);
 context.EndSkinBatch();assert(attributeLayout==1);
 picaSkinRetire(weighted.skinAllocation);picaSkinBeginFrame();frees=0;
 src[0].weights[0]=1;src[0].weights[1]=src[0].weights[2]=0;
#endif
 unsigned before=draws;p.MutateGpuSkin();assert(!p.TryGpuSkin(&material,palette,picaSkin::MaxBones));assert(draws==before);
#ifdef SRR2_OLD3DS_GPU_SKIN
 assert(p.skinAllocation);p.skinAllocation->nativeBuffer=linearAlloc(80);
 picaSkinRetire(p.skinAllocation);assert(frees==0);
 picaSkinBeginFrame();assert(frees==2 && allocations.empty());
 for(unsigned i=0;i<1000;++i){auto* a=picaSkinAllocate(56);assert(a);picaSkinRetire(a);picaSkinBeginFrame();}
#ifdef SRR2_OLD3DS_GPU_SKIN_PEDESTRIANS
 auto* pedestrian=picaSkinAllocate(picaSkin::Budget-32,true);assert(pedestrian);
 auto* player=picaSkinAllocate(picaSkin::Budget-32,false);assert(player);
#ifdef SRR2_OLD3DS_GPU_SKIN_BORROW
 auto* extraPed=picaSkinAllocate(32768-32,true);assert(extraPed);
 auto* extraPlayer=picaSkinAllocate(32768-32);assert(extraPlayer);
#ifdef SRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET
 auto* morePed=picaSkinAllocate(65536-32,true);assert(morePed);
 auto* morePlayer=picaSkinAllocate(65536-32);assert(morePlayer);
#endif
 assert(!picaSkinAllocate(1,true)&&!picaSkinAllocate(1,false)&&!picaSkinAllocate(1,false,true));
 picaSkinRetire(extraPed);picaSkinRetire(extraPlayer);
 // Retirement cannot make budget available before the GPU fence.
 assert(!picaSkinAllocate(1,true));
#ifdef SRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET
 picaSkinRetire(morePed);picaSkinRetire(morePlayer);
#endif
#else
 assert(!picaSkinAllocate(1,true)&&!picaSkinAllocate(1,false));
#endif
 picaSkinRetire(player);picaSkinRetire(pedestrian);picaSkinBeginFrame();assert(allocations.empty());
 freeSpace=picaSkin::Reserve+55;assert(!picaSkinAllocate(56,true));freeSpace=8*1024*1024;
#else
 assert(!picaSkinAllocate(1,true));
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_CAST
 auto* npc=picaSkinAllocate(picaSkin::Budget-32,false,true);assert(npc&&npc->pedestrian==2);
 auto* ped=picaSkinAllocate(picaSkin::Budget-32,true);assert(ped&&ped->pedestrian==1);
 auto* protagonist=picaSkinAllocate(picaSkin::Budget-32);assert(protagonist&&protagonist->pedestrian==0);
#ifdef SRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET
 auto* moreNpc=picaSkinAllocate(65536-32,false,true);assert(moreNpc);
 auto* morePedestrian=picaSkinAllocate(65536-32,true);assert(morePedestrian);
#endif
 assert(!picaSkinAllocate(1,false,true)&&!picaSkinAllocate(1,true)&&!picaSkinAllocate(1));
#ifdef SRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET
 picaSkinRetire(moreNpc);picaSkinRetire(morePedestrian);
#endif
 assert(!picaSkinAllocate(1,true,true));
 picaSkinRetire(npc);picaSkinRetire(ped);picaSkinRetire(protagonist);picaSkinBeginFrame();
 npc=picaSkinAllocate(picaSkin::Budget-32,false,true);assert(npc);picaSkinRetire(npc);picaSkinBeginFrame();
 freeSpace=picaSkin::Reserve+55;assert(!picaSkinAllocate(56,false,true));freeSpace=8*1024*1024;
#else
 assert(!picaSkinAllocate(1,false,true));
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_BORROW
 auto* bigNpc=picaSkinAllocate(65536-32,false,true);assert(bigNpc);
 auto* npcBorrow=picaSkinAllocate(32768-32,false,true);assert(npcBorrow);
#ifdef SRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET
 auto* moreNamed=picaSkinAllocate(65536-32,false,true);assert(moreNamed);
#endif
 assert(!picaSkinAllocate(1,false,true));
 auto* fullPlayer=picaSkinAllocate(65536-32);assert(fullPlayer);
 auto* smallPed=picaSkinAllocate(32768-32,true);assert(smallPed);
#ifdef SRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET
 auto* anotherPlayer=picaSkinAllocate(65536-32);assert(anotherPlayer);
#endif
 assert(!picaSkinAllocate(1));
 picaSkinRetire(smallPed);assert(!picaSkinAllocate(1));
 picaSkinBeginFrame();auto* playerBorrow=picaSkinAllocate(16384-32);assert(playerBorrow);
 assert(g_picaSkinMetrics.peak<=picaSkin::SharedBudget);
#ifdef SRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET
 picaSkinRetire(moreNamed);picaSkinRetire(anotherPlayer);
#endif
 picaSkinRetire(bigNpc);picaSkinRetire(npcBorrow);picaSkinRetire(fullPlayer);picaSkinRetire(playerBorrow);
 picaSkinBeginFrame();
#endif
 assert(allocations.empty());
#endif
 for(unsigned i=0;i<130;++i)picaSkinEndFrame(10,3,5,true);
 picaSkinDump();
 puts("production backend: eligibility, allocation failure, pose uploads, state restoration, mutation, retirement and 1000 reloads passed");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)
 (p/'3ds.h').write_text('#pragma once\n#include <cstdint>\nusing u64=uint64_t;using u32=uint32_t;const unsigned SYSCLOCK_ARM11=268123480;void* linearAlloc(unsigned);void linearFree(void*);unsigned linearSpaceFree();u64 svcGetSystemTick();\n')
 (p/'citro3d.h').write_text('#pragma once\nfloat C3D_GetDrawingTime();float C3D_GetProcessingTime();\n')
 (p/'test.cpp').write_text(prelude+methods+tests)
 for enabled,compact,pedestrians,batch,rigid,eye,cast,eye_cache in ((False, False, False, False, False, False, False, False), (True, False, False, False, False, False, False, False), (True, True, False, False, False, False, False, False), (True, True, True, False, False, False, False, False), (True, True, True, True, False, False, False, False), (True, False, False, True, False, False, False, False), (True, True, True, True, True, False, False, False), (True, True, True, True, True, True, False, False), (True, True, True, True, True, True, True, False), (True, True, True, True, True, True, True, True)):
  if packed and not (enabled and compact and pedestrians and batch and rigid and eye and cast and eye_cache):continue
  flags=['-DSRR2_OLD3DS_GPU_SKIN_SUPPORT','-DSRR2_OLD3DS_GPU_SKIN_METRICS']+(['-DSRR2_OLD3DS_GPU_SKIN'] if enabled else [])
  flags+=(['-DSRR2_OLD3DS_GPU_SKIN_BORROW'] if borrow else [])
  flags+=(['-DSRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET'] if extra_budget else [])
  flags+=(['-DSRR2_OLD3DS_GPU_SKIN_PACKED'] if packed else [])
  flags+=(['-DSRR2_OLD3DS_GPU_SKIN_RIGID_PACK'] if rigid else [])
  flags+=(['-DSRR2_OLD3DS_GPU_SKIN_BATCH'] if batch else [])
  flags+=(['-DSRR2_OLD3DS_GPU_SKIN_PEDESTRIANS'] if pedestrians else [])
  flags+=(['-DSRR2_OLD3DS_GPU_SKIN_EYE_CACHE'] if eye_cache else [])
  flags+=(['-DSRR2_OLD3DS_GPU_SKIN_CAST'] if cast else [])
  flags+=(['-DSRR2_OLD3DS_GPU_SKIN_EYE_PALETTE'] if eye else [])
  flags+=['-DVSH_BONE_BASE='+str(20 if eye else 24 if compact else 27)]+(['-DSRR2_OLD3DS_GPU_SKIN_COMPACT'] if compact else [])
  subprocess.run(['clang++','-std=c++11','-fsanitize=address,undefined','-I',str(p),'-I',str(ROOT/'libs/pure3d'),*flags,
      str(p/'test.cpp'),str(ROOT/'libs/pure3d/pddi/pica/skinning.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],cwd=p,check=True)
