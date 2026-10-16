#!/usr/bin/env python3
"""Compile production eligibility, upload, mutation, and retirement code with GPU spies."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
source=(ROOT/'libs/pure3d/pddi/pica/context.cpp').read_text()
def extract(signature):
    a=source.index(signature);b=source.index('{',a);depth=1;e=b+1
    while depth:
        depth+=(source[e]=='{')-(source[e]=='}');e+=1
    return source[a:e]
methods='\n'.join(extract(s) for s in ['bool picaContext::BindSkinProgram(bool enabled)',
 'void picaPrimBuffer::PrepareGpuSkin(', 'void picaPrimBuffer::MutateGpuSkin()', 'bool picaPrimBuffer::TryGpuSkin('])
prelude=r'''
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <pddi/pica/skinning.hpp>
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
C3D_FVec* C3D_FVUnifWritePtr(int,unsigned first,unsigned count){assert(first==27 && count<=69);return uniforms+first;}
void GPUCMD_GetBuffer(u32** p,u32* s,u32* o){*p=NULL;*s=10000;*o=100;}
void C3D_BindProgram(int* p){programSkin=*p==2;}
void C3D_SetAttrInfo(int*){}
const unsigned PDDI_V_NORMAL=16;
const int GPU_VERTEX_SHADER=0,VSH_FVEC_u_bones=27;
bool g_picaInFrame=true;
struct {bool bufInfoSet;} g_picaState;
bool PrimIsDrawable(int p){return p==1;}
struct picaPrimBuffer;
struct picaContext {
 void* skinBinary=(void*)1;int skinProgram=2,shaderProgram=1,skinAttrInfo=2,attrInfo=1;
 unsigned invalidations=0;
 void InvalidateUniformCache(){++invalidations;}
 bool BindSkinProgram(bool);
 void DrawPrimBuffer(pddiShader*,picaPrimBuffer*);
};
struct picaPrimBuffer {
 picaContext* context;bool transient=false;unsigned char* buffer;unsigned allocated=2;
 int primType=1;unsigned vertexType=16,mem=80;bool validIndices=true;
 bool IndicesInRange(){return validIndices;}
 picaSkinAllocation* skinAllocation=NULL;picaSkin::Reason skinReason=picaSkin::NotSelected;
 unsigned skinBoneCount=0;bool skinDrawing=false;
 void PrepareGpuSkin(const void*,unsigned,unsigned,bool);void MutateGpuSkin();
 bool TryGpuSkin(pddiShader*,const pddiMatrix* const*,unsigned);
};
void picaContext::DrawPrimBuffer(pddiShader*,picaPrimBuffer* p){assert(programSkin && p->skinDrawing);++draws;++materialCalls;}
'''
tests=r'''
int main(){
 picaContext context;pddiShader material;unsigned char buffer[72]={};picaSkin::Source src[2]={};
 for(auto& v:src){v.weights[0]=1;v.normal[2]=1;}
 pddiMatrix matrices[23]={};const pddiMatrix* palette[23];
 for(unsigned i=0;i<23;++i){palette[i]=&matrices[i];for(unsigned j=0;j<4;++j)matrices[i].rows[j][j]=1;matrices[i].rows[3][0]=float(i);}
 auto make=[&](){picaPrimBuffer p;p.context=&context;p.buffer=buffer;return p;};
 picaPrimBuffer unsupported=make();unsupported.PrepareGpuSkin(src,2,24,true);assert(unsupported.skinReason==picaSkin::Palette);
 unsupported=make();unsupported.validIndices=false;unsupported.PrepareGpuSkin(src,2,2,true);assert(unsupported.skinReason==picaSkin::Topology);
 unsupported=make();src[0].indices[0]=23;unsupported.PrepareGpuSkin(src,2,23,true);assert(unsupported.skinReason==picaSkin::SourceInvalid);src[0].indices[0]=0;
 failAllocation=true;unsupported=make();unsupported.PrepareGpuSkin(src,2,2,true);
#ifdef SRR2_OLD3DS_GPU_SKIN
 assert(!unsupported.skinAllocation && unsupported.skinReason!=picaSkin::Ready);
#endif
 failAllocation=false;
 picaPrimBuffer p=make();p.PrepareGpuSkin(src,2,23,true);assert(p.skinReason==picaSkin::Ready);
 unsigned initialFlushes=flushes;
 for(unsigned pose=0;pose<10;++pose){
  matrices[22].rows[3][0]=float(pose);g_picaState.bufInfoSet=true;
  bool used=p.TryGpuSkin(&material,palette,23);
#ifdef SRR2_OLD3DS_GPU_SKIN
  assert(used && !programSkin && !g_picaState.bufInfoSet && !p.skinDrawing);
  assert(uniforms[27+66].w==float(pose));
#else
  assert(!used && !draws);
#endif
 }
 assert(flushes==initialFlushes);
 matrices[0].rows[0][0]=std::numeric_limits<float>::infinity();
 assert(!p.TryGpuSkin(&material,palette,23));matrices[0].rows[0][0]=1;
 g_picaInFrame=false;assert(!p.TryGpuSkin(&material,palette,23));g_picaInFrame=true;
 unsigned before=draws;p.MutateGpuSkin();assert(!p.TryGpuSkin(&material,palette,23));assert(draws==before);
#ifdef SRR2_OLD3DS_GPU_SKIN
 assert(p.skinAllocation);p.skinAllocation->nativeBuffer=linearAlloc(80);
 picaSkinRetire(p.skinAllocation);assert(frees==0);
 picaSkinBeginFrame();assert(frees==2 && allocations.empty());
 for(unsigned i=0;i<1000;++i){auto* a=picaSkinAllocate(56);assert(a);picaSkinRetire(a);picaSkinBeginFrame();}
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
 for enabled in (False,True):
  flags=['-DSRR2_OLD3DS_GPU_SKIN_SUPPORT','-DSRR2_OLD3DS_GPU_SKIN_METRICS']+(['-DSRR2_OLD3DS_GPU_SKIN'] if enabled else [])
  subprocess.run(['clang++','-std=c++11','-fsanitize=address,undefined','-I',str(p),'-I',str(ROOT/'libs/pure3d'),*flags,
      str(p/'test.cpp'),str(ROOT/'libs/pure3d/pddi/pica/skinning.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],cwd=p,check=True)
