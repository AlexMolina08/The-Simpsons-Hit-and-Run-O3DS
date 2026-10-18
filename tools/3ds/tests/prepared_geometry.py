#!/usr/bin/env python3
"""Compare both production Display branches using the same selected geometry."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
def clean(s):return '\n'.join(x for x in s.splitlines() if not x.startswith('#include'))+'\n'
def extract(s,signature):
 a=s.index(signature);b=s.index('{',a);end=b+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]+'\n'
source=r'''
#include <array>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstdio>
using u32=uint32_t;using u64=uint64_t;using GPU_Primitive_t=int;
#define SRR2_OLD3DS_PREPARED_BACKEND
#define SRR2_OLD3DS_DRAW_PREP
#define SRR2_OLD3DS_DRAW_PACKET
#define SRR2_OLD3DS_GPU_SKIN_SUPPORT
#define SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
#define SRR2_OLD3DS_WHEEL_SKIN_BATCH
#define SRR2_OLD3DS_WHEEL_GPU_BATCH
#define SRR2_OLD3DS_NO_NORMAL_FETCH
#define SRR2_OLD3DS_SKIN_RING
#define SRR2_OLD3DS_SHADOW_MESH
constexpr unsigned PICA_VERTEX_STRIDE=36,PICA_BUFINFO_BASE=0x14000000,GPUREG_ATTRIBBUFFER0_OFFSET=0x200,C3D_UNSIGNED_SHORT=1;
namespace picaSkin{constexpr unsigned VertexStride=24;unsigned PackedStride(bool rigid){return rigid?20:24;}}
namespace picaNoNormal{constexpr unsigned Stride=24;}
namespace picaWheelLOD{struct PaletteVertex{unsigned char data[40];};}
std::vector<std::array<uintptr_t,8>> trace;
struct C3D_BufInfo{const void* vertices;unsigned stride,attributes;u64 permutation;};
void BufInfo_Init(C3D_BufInfo* b){*b={};trace.push_back({0});}
void BufInfo_Add(C3D_BufInfo* b,const void* v,unsigned s,unsigned a,u64 p){*b={v,s,a,p};trace.push_back({1,uintptr_t(v),s,a,p});}
void C3D_SetBufInfo(const C3D_BufInfo* b){trace.push_back({2,uintptr_t(b->vertices),b->stride,b->attributes,b->permutation});}
extern "C" void C3Di_UpdateContext(){trace.push_back({3});}
void GPUCMD_AddWrite(unsigned r,unsigned value){trace.push_back({4,r,value});}
void picaDrawPacketElements(int p,int count,int type,const void* indices){trace.push_back({5,uintptr_t(p),uintptr_t(count),uintptr_t(type),uintptr_t(indices)});}
void picaDrawPacketArrays(int p,int first,int count){trace.push_back({6,uintptr_t(p),uintptr_t(first),uintptr_t(count)});}
unsigned commandOffset=0,commandSize=8192;bool commandAvailable=true;
void GPUCMD_GetBuffer(u32** buffer,u32* size,u32* offset){static u32 storage[8192];*buffer=commandAvailable?storage:nullptr;*size=commandSize;*offset=commandOffset;}
u32 osConvertVirtToPhys(const void* address){return PICA_BUFINFO_BASE+(uintptr_t(address)&0xffffff);}
struct {bool bufInfoSet=false;} g_picaState;
bool g_picaInFrame=true,g_picaPreparedBackendEnabled=false;
struct picaPreparedGeometry;
struct picaContext{
 bool normal=false,wheel=false;bool PreparedBatching(){return false;}bool PreparedGeometryRoom(){return true;}void QueuePreparedGeometry(const picaPreparedGeometry&){}
 void ApplyViewport(){trace.push_back({7});}
 void SetNoNormalLayout(bool b){if(normal!=b){normal=b;g_picaState.bufInfoSet=false;trace.push_back({8,b});}}
 void SetWheelPaletteLayout(bool b){if(wheel!=b){wheel=b;g_picaState.bufInfoSet=false;trace.push_back({9,b});}}
};
GPU_Primitive_t primTypeTable[5]={0,1,2,3,4};
bool PrimIsDrawable(unsigned p){return p<4;}
struct Allocation{unsigned char data[32];};
'''
source+=clean((root/'libs/pure3d/pddi/pica/preparedbackend.h').read_text())+'picaPreparedBackendStats preparedBackendStats={};\n'+clean((root/'libs/pure3d/pddi/pica/preparedgeometry.hpp').read_text())
source+=r'''
struct picaPrimBuffer{
 picaContext* context;unsigned char* buffer=nullptr;unsigned char* skinVertices=nullptr;Allocation* skinAllocation=nullptr;
 unsigned short* indices=nullptr;unsigned char* expanded=nullptr;
 unsigned primType=0,count=0,indexCount=0,total=0,allocated=0;
 bool skinDrawing=false,wheelSkinBuffer=false,transient=false,drawPrepared=false;
 bool noNormalPacked=false,wheelPaletteBuffer=false,skinRigid=false,expansionFails=false,canPrepare=false;
 int elemState=-1;unsigned nativeCount=0;GPU_Primitive_t nativePrimitive=0;u32 nativeOffset=0;
 unsigned prepares=0,expansions=0,scans=0;
 bool IndicesInRange(){++scans;return indices&&indexCount&&allocated&&indices[0]<allocated;}
 bool BuildExpanded(){++expansions;return !expansionFails;}
 void PrepareDraw(){++prepares;if(canPrepare){drawPrepared=true;nativeCount=indexCount;nativePrimitive=primTypeTable[primType];nativeOffset=osConvertVirtToPhys(buffer)-PICA_BUFINFO_BASE;}}
 bool CapturePreparedGeometry(picaPreparedGeometry&);void Display();
};
'''
production=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
source+=extract(production,'bool picaPrimBuffer::CapturePreparedGeometry(')+extract(production,'void picaPrimBuffer::Display(void)')
source+=r'''
unsigned seed=735;unsigned randomValue(){seed=seed*1664525u+1013904223u;return seed;}
int main(){
 std::array<unsigned char,4096> vertices,ring,expanded;std::array<unsigned short,64> indices={};Allocation skin[256];
 picaContext referenceContext,preparedContext;bool referenceBound=false,preparedBound=false;
 unsigned draws=0;
 for(unsigned n=0;n<30000;++n){
  picaPrimBuffer reference;reference.context=&referenceContext;reference.buffer=n%103?vertices.data():nullptr;
  reference.indices=n%3?indices.data():nullptr;reference.indexCount=randomValue()%65;reference.total=randomValue()%65;reference.allocated=randomValue()%65;
  indices[0]=randomValue()%70;reference.elemState=n%5?int(randomValue()%3)-1:-1;
  reference.expanded=expanded.data();reference.expansionFails=n%41==0;reference.transient=n%7==0;
  reference.skinAllocation=skin;reference.skinDrawing=n%11==0;reference.wheelSkinBuffer=n%13==0;
  reference.skinVertices=n%17==0?ring.data():nullptr;reference.primType=randomValue()%5;
  reference.noNormalPacked=n%19==0;reference.wheelPaletteBuffer=n%23==0;reference.skinRigid=n%2;
  reference.drawPrepared=n%29==0;reference.nativeCount=reference.indexCount;reference.nativePrimitive=reference.primType;reference.nativeOffset=osConvertVirtToPhys(reference.buffer)-PICA_BUFINFO_BASE;
  reference.canPrepare=n%31==0;
  g_picaInFrame=n%101!=0;commandAvailable=n%71!=0;commandOffset=n%67?0:8000;
  if(n%37==0)referenceBound=preparedBound=false;
  picaPrimBuffer prepared=reference;prepared.context=&preparedContext;
  trace.clear();g_picaPreparedBackendEnabled=false;g_picaState.bufInfoSet=referenceBound;reference.Display();referenceBound=g_picaState.bufInfoSet;auto expected=trace;
  trace.clear();g_picaPreparedBackendEnabled=true;g_picaState.bufInfoSet=preparedBound;prepared.Display();preparedBound=g_picaState.bufInfoSet;
  assert(expected==trace);assert(referenceBound==preparedBound);assert(referenceContext.normal==preparedContext.normal);assert(referenceContext.wheel==preparedContext.wheel);
  assert(reference.drawPrepared==prepared.drawPrepared);assert(reference.elemState==prepared.elemState);assert(reference.prepares==prepared.prepares);assert(reference.expansions==prepared.expansions);assert(reference.scans==prepared.scans);
  for(const auto& call:trace)draws+=call[0]==5||call[0]==6;
 }
 printf("PASS: 30000 production Display sequences match legacy binding/submission order; %u submitted draws. Covers indexed/expanded/transient, empty/failure, skin/ring/normal/wheel layouts and command room. Mock SDK and geometry preparation helpers; not GPU output, lifetime or FPS proof.\n",draws);
}
'''
with tempfile.TemporaryDirectory(prefix='prepared-geometry-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
