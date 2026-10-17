#!/usr/bin/env python3
"""Compare production indexed character shadows against the original ordered triangles."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'code/worldsim/character/characterrenderable.cpp').read_text()
c=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
def extract(source,signature):
 start=source.index(signature);end=source.index('{',start)+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
legacy=extract(s,'for(int i=0; i < NumBlobSlices; i++)')
code=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
#include <cmath>
#include "libs/pure3d/pddi/pica/blobtopology.hpp"
struct pddiVector {float x,y,z;void Set(float a,float b,float c){x=a;y=b;z=c;}};
using pddiColour=unsigned;struct pddiShader {};
struct Vertex {pddiVector point;pddiColour colour;};
struct pddiPrimStream {unsigned colour=0;std::vector<Vertex> vertices;
 void Colour(unsigned c){colour=c;}void Coord(float x,float y,float z){vertices.push_back({{x,y,z},colour});}
 void Vertex(pddiVector* v,unsigned c){vertices.push_back({*v,c});}};
static const unsigned PICA_VERTEX_STRIDE=36,PICA_IMM_VERTEX_BYTES=512*1024,PICA_IMM_INDEX_BYTES=65536;
static const unsigned PDDI_PRIM_TRIANGLES=0,PDDI_V_C=1;
struct Buffer {unsigned short* indices=nullptr;unsigned count=0;void SetTransientIndices(unsigned short* p,unsigned n){indices=p;count=n;}};
struct picaContext {unsigned immVertexUsed=0,immIndexUsed=0,ends=0;bool failBegin=false;
 Buffer buffer;Buffer* theImmBuffer=&buffer;pddiPrimStream stream;unsigned short indices[65536/2]={};
 unsigned short* AllocImmIndices(unsigned n){auto* p=indices+immIndexUsed/2;immIndexUsed+=n*2;return p;}
 pddiPrimStream* BeginPrims(pddiShader*,unsigned,unsigned,unsigned n){if(failBegin)return nullptr;
  immVertexUsed+=n*36;stream.vertices.clear();return &stream;}
 void EndPrims(pddiPrimStream* p){assert(p==&stream);++ends;}
 bool DrawCharacterShadow(pddiShader*,const pddiVector*,pddiColour,pddiColour);
};
'''+extract(s,'void BuildCharacterShadowVertices(')+extract(c,'bool picaContext::DrawCharacterShadow(')+r'''
int main(){std::mt19937 rng(90231);std::uniform_real_distribution<float> value(-2,2);
 const int NumBlobSlices=16,HalfCircle=8;const float NonArticulatedScale=.2f;
 for(int trial=0;trial<10000;++trial){float BlobPoints[8][2],FadePoints[8][2],pointScales[16];
  for(auto& p:BlobPoints)for(auto& v:p)v=value(rng);for(auto& p:FadePoints)for(auto& v:p)v=value(rng);
  for(auto& v:pointScales)v=value(rng);
  bool articulated=trial%2;struct Params {float ShadowScale;} params{value(rng)};auto* BlobParams=&params;
  if(trial%19==0)params.ShadowScale=-0.0f;if(trial%23==0)params.ShadowScale=0.0f;
  unsigned insideColour=rng(),OutsideColour=rng();pddiPrimStream reference;auto* blob=&reference;
'''+legacy+r'''
  pddiVector vertices[picaCharacterBlob::VertexCount];
  BuildCharacterShadowVertices(vertices,BlobPoints,FadePoints,pointScales,params.ShadowScale,articulated);
  picaContext context;pddiShader shader;assert(context.DrawCharacterShadow(&shader,vertices,insideColour,OutsideColour));
  assert(context.ends==1&&context.stream.vertices.size()==33&&context.buffer.count==144);
  assert(reference.vertices.size()==144);
  for(unsigned i=0;i<144;++i){const auto& a=reference.vertices[i];const auto& b=context.stream.vertices.at(context.buffer.indices[i]);
   assert(a.colour==b.colour&&!memcmp(&a.point,&b.point,sizeof(a.point)));}
  auto saved=context.stream.vertices;unsigned short* previous=context.buffer.indices;
  assert(context.DrawCharacterShadow(&shader,vertices,insideColour,OutsideColour));
  assert(context.buffer.indices!=previous&&context.immVertexUsed==2*33*36&&context.immIndexUsed==2*144*2);
  for(unsigned i=0;i<144;++i)assert(previous[i]==context.buffer.indices[i]);
 }
 picaContext c;pddiVector vertices[33]={};pddiShader shader;
 c.immVertexUsed=PICA_IMM_VERTEX_BYTES-33*36+1;assert(!c.DrawCharacterShadow(&shader,vertices,1,2)&&!c.ends&&!c.immIndexUsed);
 c.immVertexUsed=0;c.immIndexUsed=PICA_IMM_INDEX_BYTES-144*2+1;assert(!c.DrawCharacterShadow(&shader,vertices,1,2)&&!c.ends&&!c.immVertexUsed);
 c.immIndexUsed=0;c.failBegin=true;assert(!c.DrawCharacterShadow(&shader,vertices,1,2)&&!c.ends);
 puts("PASS: 10,000 production character shadows; 144 ordered vertices/colors bit-exact through 33 indexed vertices; ring ownership, exhaustion and BeginPrims failure");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-character-shadow-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
