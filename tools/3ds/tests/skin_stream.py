#!/usr/bin/env python3
"""Execute production stream allocation, flush, expansion and draw submission."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
source=(ROOT/'libs/pure3d/pddi/pica/context.cpp').read_text()
def extract(signature):
 start=source.index(signature);opening=source.index('{',start);depth=1;end=opening+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
prelude=r'''
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <cstdint>
#define SRR2_OLD3DS_SKIN_RING
using u32=uintptr_t;
using GPU_Primitive_t=unsigned;
const unsigned PICA_VERTEX_STRIDE=36,PICA_IMM_VERTEX_BYTES=512*1024,PICA_IMM_BUFFERS=2;
const unsigned PICA_BUFINFO_BASE=0,GPUREG_ATTRIBBUFFER0_OFFSET=0,C3D_UNSIGNED_SHORT=0;
const unsigned primTypeTable[]={0,1};
int g_picaInFrame=1;
bool PrimIsDrawable(unsigned p){return p<2;}
struct {bool bufInfoSet=false;} g_picaState;
static const void* drawBase;static unsigned draws,count,flushCount;
static std::vector<const void*> flushed;
void GSPGPU_FlushDataCache(const void* p,unsigned){++flushCount;flushed.push_back(p);}
void* linearAlloc(unsigned n){return malloc(n);}void linearFree(void* p){free(p);}
void* picaBufferAllocate(unsigned n){return malloc(n);}
void picaBufferRetire(void* p){free(p);}
void picaFlushForGpu(const void* p,unsigned n){GSPGPU_FlushDataCache(p,n);}
void GPUCMD_GetBuffer(u32** p,u32* size,u32* offset){static u32 command;*p=&command;*size=999999;*offset=0;}
void GPUCMD_AddWrite(unsigned,u32 address){drawBase=(const void*)address;}
uintptr_t osConvertVirtToPhys(const void* p){return (uintptr_t)p;}
struct C3D_BufInfo{};
void BufInfo_Init(C3D_BufInfo*){}
void BufInfo_Add(C3D_BufInfo*,const void*,unsigned,unsigned,unsigned){}
void C3D_SetBufInfo(C3D_BufInfo*){}
void C3Di_UpdateContext(){}
void C3D_DrawElements(unsigned,int n,unsigned,const void*){++draws;count=n;}
void C3D_DrawArrays(unsigned,int,int n){++draws;count=n;}
struct picaContext {
 unsigned char* immVertexRing[2];unsigned char* immVertices;unsigned short* immIndices;
 unsigned immRing=1,immVertexUsed=0,immIndexUsed=0,skinVertexUsed=0;
 unsigned char* AllocSkinVertices(unsigned);void FlushStreams();void ApplyViewport(){}
};
struct picaPrimBuffer {
 picaContext* context;unsigned char* buffer;unsigned char* skinVertices=nullptr;
 unsigned char* expanded=nullptr;unsigned expandedBytes=0,mem=0;
 unsigned allocated=3,total=3,indexCount=3,primType=0;bool expandDirty=true,transient=false;
 unsigned short* indices=nullptr;int elemState=-1;
 bool IndicesInRange()const{for(unsigned i=0;i<indexCount;++i)if(indices[i]>=allocated)return false;return true;}
 bool drawPrepared=false;GPU_Primitive_t nativePrimitive=0;u32 nativeOffset=0;unsigned nativeCount=0;
 void PrepareDraw();bool BuildExpanded();void Display();
};
'''
tests=r'''
int main(){
 std::vector<unsigned char> effect(PICA_IMM_VERTEX_BYTES,0x11),skin(PICA_IMM_VERTEX_BYTES,0x22),legacy(3*36,0x33);
 unsigned short indices[]={0,1,2};picaContext c;c.immVertexRing[0]=skin.data();c.immVertexRing[1]=effect.data();c.immVertices=effect.data();c.immIndices=indices;
 picaPrimBuffer b{&c,legacy.data()};b.indices=indices;
 auto* first=c.AllocSkinVertices(3);assert(first==skin.data());memset(first,0x44,3*36);b.skinVertices=first;
 b.Display();assert(drawBase==first&&draws==1&&count==3);
 auto* second=c.AllocSkinVertices(3);assert(second==first+3*36);memset(second,0x55,3*36);b.skinVertices=second;
 b.Display();assert(drawBase==second&&draws==2&&count==3);
 for(unsigned i=0;i<3*36;++i)assert(first[i]==0x44);
 assert(c.immVertexUsed==0&&effect[0]==0x11);
 unsigned used=c.skinVertexUsed;
 assert(!c.AllocSkinVertices(PICA_IMM_VERTEX_BYTES/36));assert(c.skinVertexUsed==used);
 assert(!c.AllocSkinVertices(UINT32_MAX));assert(c.skinVertexUsed==used);
 c.immVertexUsed=36;c.immIndexUsed=6;c.FlushStreams();
 assert(flushCount==3&&flushed[0]==effect.data()&&flushed[1]==indices&&flushed[2]==skin.data());
 // Mutation to an invalid index must expand the most recent skinned output.
 indices[2]=65535;b.elemState=-1;b.drawPrepared=false;b.Display();assert(drawBase==b.expanded&&count==3);
 for(unsigned i=0;i<3*36;++i)assert(b.expanded[i]==0x55);
 // Legacy fallback draws the persistent buffer and uses its own ownership.
 b.skinVertices=nullptr;indices[2]=2;b.elemState=-1;b.drawPrepared=false;b.Display();assert(drawBase==legacy.data());
 g_picaInFrame=0;unsigned before=draws;b.Display();assert(draws==before);
 free(b.expanded);
 puts("production skin stream: draw addresses, immutable snapshots, separate effect capacity, overflow, flushes, invalid-index expansion and legacy fallback passed");
}
'''
methods='#ifdef SRR2_OLD3DS_DRAW_PREP\n'+extract('void picaPrimBuffer::PrepareDraw()')+'\n#endif\n'+'\n'.join(extract(sig) for sig in ['void picaContext::FlushStreams()','unsigned char* picaContext::AllocSkinVertices','bool picaPrimBuffer::BuildExpanded','void picaPrimBuffer::Display'])
with tempfile.TemporaryDirectory(prefix='srr-skin-stream-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+methods+tests)
 for mode in ['reference','prepared']:
  subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')]+(['-DSRR2_OLD3DS_DRAW_PREP'] if mode=='prepared' else []),check=True)
  subprocess.run([str(p/'test')],check=True)
