#!/usr/bin/env python3
"""Exercise production ordered static-batch baking, lifetime and draw guards."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
backend=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
start=backend.index('#ifdef SRR2_OLD3DS_STATIC_BATCH\nbool picaPrimBuffer::StaticBatchSource')
end=backend.index('\npicaPrimBuffer::~picaPrimBuffer()',start)
production=backend[start:end]
source=r'''
#include <pddi/pica/staticbatch.hpp>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#define SRR2_OLD3DS_STATIC_BATCH
#define SRR2_OLD3DS_LOAD_UPLOAD
#define SRR2_OLD3DS_GPU_SKIN_SUPPORT
#define SRR2_OLD3DS_SKIN_RING
#define SRR2_OLD3DS_WHEEL_GPU_BATCH
#define SRR2_OLD3DS_WHEEL_SKIN_BATCH
#define SRR2_OLD3DS_NO_NORMAL_FETCH
constexpr unsigned PDDI_V_NORMAL=1;
constexpr int PDDI_PRIM_TRIANGLES=0,PDDI_PRIM_TRISTRIP=1,PDDI_COMPARE_LESS=0,PDDI_COMPARE_LESSEQUAL=1;
static unsigned freeBytes=16*1024*1024,alive=0,flushes=0;
static bool g_picaInFrame=true;
unsigned linearSpaceFree(){return freeBytes;}
void picaFlushForGpu(const void*,unsigned){++flushes;}
class picaContext;
struct pddiShader {bool opaque=true;};
struct picaMat:pddiShader {bool StaticBatchOpaque()const{return opaque;}};
class picaPrimBuffer {
public:
 picaContext* context;int primType;unsigned vertexType,allocated,indexCount,mem,total=0;
 unsigned char* buffer;unsigned short* indices;unsigned wheelSourceVersion=1;
 bool transient=false,uploadPending=false,skinDrawing=false,wheelPaletteBuffer=false,wheelSkinBuffer=false,noNormalPacked=false,culled=false;
 void* skinAllocation=nullptr;void* skinVertices=nullptr;int elemState=1,refs=0;
 picaPrimBuffer(picaContext* c,int type,unsigned format,unsigned vertices,unsigned count):context(c),primType(type),vertexType(format),allocated(vertices),indexCount(count),mem(vertices*36+((count*2+3)&~3u)) {
  ++alive;buffer=new unsigned char[mem]();indices=reinterpret_cast<unsigned short*>(buffer+vertices*36);
 }
 ~picaPrimBuffer(){delete[]buffer;--alive;}
 void AddRef(){++refs;}void Release(){assert(refs>0);if(!--refs)delete this;}
 unsigned WheelSourceVersion()const{return wheelSourceVersion;}
 bool CullNative(){return culled;}
 void PrepareDraw(){}
 bool StaticBatchSource()const;
 static unsigned StaticBatchSize(picaPrimBuffer* const*,unsigned,unsigned&,unsigned&);
 static picaPrimBuffer* BakeStaticBatch(picaPrimBuffer* const*,unsigned);
};
struct Render {bool zEnabled=true,zWrite=true;int zCompare=PDDI_COMPARE_LESS;};
struct Stencil {bool enabled=false;};
class picaContext {
public:
 bool bottomPass=false,skinProgramBound=false;Render render;Stencil stencil;
 struct {Render* renderState;Stencil* stencilState;}state{&render,&stencil};
 picaStaticBatch::Entry<picaPrimBuffer> staticBatches[picaStaticBatch::Slots];
 unsigned staticBatchFrame=0,staticBatchBuildFrame=~0u;
 unsigned staticBatchBuilt=0,staticBatchHits=0,staticBatchAvoided=0,staticBatchMisses=0,staticBatchFailures=0;
 unsigned staticBatchBytes=0,staticBatchPeak=0;
 unsigned draws=0;picaPrimBuffer* submitted=nullptr;pddiShader* shader=nullptr;
 void DrawPrimBuffer(pddiShader* m,picaPrimBuffer* b){++draws;submitted=b;shader=m;}
 bool DrawStaticBatch(pddiShader*,picaPrimBuffer* const*,unsigned);
 void ClearStaticBatches();
 ~picaContext(){ClearStaticBatches();}
};
'''+production+r'''
static unsigned randomValue=19;
unsigned nextRandom(){return randomValue=randomValue*1664525u+1013904223u;}
int main(){
 unsigned trianglesTested=0,verticesTested=0;
 for(unsigned trial=0;trial<2400;++trial){
  picaContext context;std::vector<picaPrimBuffer*> originals;
  const unsigned count=2+nextRandom()%15;
  std::vector<std::vector<unsigned char>> snapshots;
  for(unsigned group=0;group<count;++group){
   const unsigned vertices=3+nextRandom()%28,triangles=1+nextRandom()%12;const bool strip=nextRandom()&1,packed=nextRandom()&1;
   auto* b=new picaPrimBuffer(&context,strip?PDDI_PRIM_TRISTRIP:PDDI_PRIM_TRIANGLES,0,vertices,strip?triangles+2:triangles*3);b->AddRef();
   b->noNormalPacked=packed;
   for(unsigned v=0;v<vertices;++v){
    float data[9];for(auto& value:data)value=float(int(nextRandom()%20001)-10000)*.125f;
    std::memcpy(b->buffer+v*(packed?24:36),data,packed?24:36);
   }
   for(unsigned i=0;i<b->indexCount;++i)b->indices[i]=nextRandom()%vertices;
   snapshots.emplace_back(b->buffer,b->buffer+b->mem);originals.push_back(b);
  }
  auto* baked=picaPrimBuffer::BakeStaticBatch(originals.data(),count);assert(baked);baked->AddRef();
  unsigned output=0;
  for(unsigned group=0;group<count;++group){
   const auto* b=originals[group];const bool strip=b->primType==PDDI_PRIM_TRISTRIP;
   unsigned tris=strip?b->indexCount-2:b->indexCount/3;
   for(unsigned t=0;t<tris;++t){
    unsigned positions[3]={strip?t:t*3,strip?t+1:t*3+1,strip?t+2:t*3+2};
    if(strip && (t&1))std::swap(positions[0],positions[1]);
    for(unsigned corner:positions){
     const auto* actual=baked->buffer+baked->indices[output++]*36;
     const auto* expected=b->buffer+b->indices[corner]*(b->noNormalPacked?24:36);
     assert(!std::memcmp(actual,expected,12));
     assert(!std::memcmp(actual+24,expected+(b->noNormalPacked?12:24),12));
     if(!b->noNormalPacked)assert(!std::memcmp(actual+12,expected+12,12));
     else {float normal[3]={0,0,1};assert(!std::memcmp(actual+12,normal,12));}
     ++verticesTested;
    }++trianglesTested;
   }
   assert(!std::memcmp(b->buffer,snapshots[group].data(),b->mem));
  }
  assert(output==baked->indexCount);baked->Release();
  picaMat material;
  for(unsigned frame=1;frame<=4;++frame){
   context.staticBatchFrame=frame;const unsigned before=context.draws;
   bool drawn=context.DrawStaticBatch(&material,originals.data(),count);
   assert(drawn==(frame>=3));assert(context.draws-before==unsigned(drawn));
  }
  assert(context.staticBatchBuilt==1 && context.staticBatchAvoided==2*(count-1));
  assert(context.shader==&material);
  auto guard=[&](){const unsigned draws=context.draws,flush=flushes;assert(!context.DrawStaticBatch(&material,originals.data(),count));assert(context.draws==draws && flushes==flush);};
  context.render.zWrite=false;guard();context.render.zWrite=true;
  context.render.zEnabled=false;guard();context.render.zEnabled=true;
  context.render.zCompare=2;guard();context.render.zCompare=PDDI_COMPARE_LESSEQUAL;
  context.stencil.enabled=true;guard();context.stencil.enabled=false;
  context.bottomPass=true;guard();context.bottomPass=false;
  context.skinProgramBound=true;guard();context.skinProgramBound=false;
  material.opaque=false;guard();material.opaque=true;
  g_picaInFrame=false;guard();g_picaInFrame=true;
  auto* b=originals[0];
  b->culled=true;guard();b->culled=false;
  b->uploadPending=true;guard();b->uploadPending=false;
  b->skinAllocation=b;guard();b->skinAllocation=nullptr;
  b->skinVertices=b;guard();b->skinVertices=nullptr;
  b->wheelPaletteBuffer=true;guard();b->wheelPaletteBuffer=false;
  b->wheelSkinBuffer=true;guard();b->wheelSkinBuffer=false;
  b->transient=true;guard();b->transient=false;
  b->elemState=0;guard();b->elemState=1;
  ++b->wheelSourceVersion;guard(); // A changed allocation cannot hit the baked copy.
  b->wheelSourceVersion=~0u;guard();
  context.ClearStaticBatches();assert(!context.staticBatchBytes);
  for(auto* original:originals){assert(original->refs==1);original->Release();}
 }
 assert(!alive);
 // Explicit overflow, invalid-index, memory-pressure and list-tail fallbacks.
 picaContext context;
 auto* a=new picaPrimBuffer(&context,PDDI_PRIM_TRIANGLES,0,3,3);a->AddRef();
 auto* b=new picaPrimBuffer(&context,PDDI_PRIM_TRISTRIP,0,3,3);b->AddRef();
 for(unsigned i=0;i<3;++i)a->indices[i]=b->indices[i]=i;
 picaPrimBuffer* pair[2]={a,b};
 freeBytes=0;assert(!picaPrimBuffer::BakeStaticBatch(pair,2));freeBytes=16*1024*1024;
 b->indices[2]=3;assert(!picaPrimBuffer::BakeStaticBatch(pair,2));b->indices[2]=2;
 a->indexCount=2;assert(!picaPrimBuffer::BakeStaticBatch(pair,2));a->indexCount=3;
 unsigned short out[3],in[3]={0,1,2};
 assert(!picaStaticBatch::AppendIndices(out,in,3,3,65534,false));
 assert(picaStaticBatch::AppendIndices(out,in,3,3,65533,false));assert(out[2]==65535);
 picaMat material;for(unsigned frame=1;frame<=2;++frame){context.staticBatchFrame=frame;assert(!context.DrawStaticBatch(&material,pair,2));}
 freeBytes=0;context.staticBatchFrame=3;assert(!context.DrawStaticBatch(&material,pair,2));assert(context.staticBatchFailures==1 && !context.draws);
 freeBytes=16*1024*1024;context.staticBatchFrame=4;assert(context.DrawStaticBatch(&material,pair,2));
 // Cache owns sources independently of their original geometry/streaming owners.
 a->Release();b->Release();assert(alive==3);context.ClearStaticBatches();assert(!alive);
 printf("PASS: %u ordered triangles / %u vertex corners; 2400 production bake/cache cases, mutation, ownership, source-byte fidelity, state and allocation fallbacks. Not native performance evidence.\n",trianglesTested,verticesTested);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-static-batch-') as td:
 p=Path(td);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
