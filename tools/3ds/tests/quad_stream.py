#!/usr/bin/env python3
"""Exercise production quad reservation/emission/end methods and exact topology."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
h=(root/'libs/pure3d/pddi/pica/context.hpp').read_text()
def extract(text,sig):
 a=text.index(sig);b=text.index('{',a)+1;e=b;depth=1
 while depth:
  depth+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[a:e]
direct='void picaPrimBuffer::AppendQuad' in s
methods='\n'.join(extract(s,x) for x in [
 'unsigned char* picaContext::AllocImmVertices(unsigned count)',
 'unsigned short* picaContext::AllocImmIndices(unsigned count)',
 'pddiPrimStream* picaContext::BeginQuadStream(pddiShader* shader,unsigned count)',
 'bool picaContext::EmitQuad(pddiPrimStream* stream,const pddiVector* positions,',
 'void picaContext::EndPrims(pddiPrimStream* stream)',
 'void picaContext::FlushStreams()'])
coloured='void picaPrimBuffer::AppendColourQuad' in s
if coloured: methods+='\n'+extract(s,'void picaPrimBuffer::AppendColourQuad').replace('picaPrimBuffer::','Buffer::')+'\n'+extract(s,'bool picaContext::EmitColourQuad')
if direct: methods+='\n'+extract(s,'void picaPrimBuffer::AppendQuad').replace('picaPrimBuffer::','Buffer::')
prelude=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <array>
#define SRR2_OLD3DS_QUAD_STREAM
#include <pddi/pica/quadstream.hpp>
const unsigned PICA_VERTEX_STRIDE=36,PICA_IMM_VERTEX_BYTES=4096,PICA_IMM_INDEX_BYTES=1024;
const int PDDI_PRIM_TRIANGLES=0,PDDI_V_CT=1;
const unsigned PICA_VERTEX_OFF_POS=0,PICA_VERTEX_OFF_UV=24,PICA_VERTEX_OFF_COL=32;
#define PDDIASSERT assert
bool g_picaInFrame=true;unsigned flushCount=0,flushedBytes=0;
void GSPGPU_FlushDataCache(void*,unsigned n){++flushCount;flushedBytes+=n;}
struct Record {pddiVector position;float normal[3]={0,0,1};float uv[2];unsigned char colour[4];};
static_assert(sizeof(Record)==36,"Native vertex layout");
struct Buffer;
struct pddiPrimStream {Buffer* buffer=nullptr;unsigned calls=0;pddiPrimStream* stream=this;void Vertex(pddiVector*,pddiColour,pddiVector2*);};
struct Buffer {
 unsigned char* data=nullptr;unsigned char* buffer=nullptr;float* coord=nullptr;float* normal=nullptr;float* uv=nullptr;unsigned char* colour=nullptr;bool transient=true;int vertexType=PDDI_V_CT;
 void AppendQuad(const pddiVector*,const pddiVector2*,pddiColour);
 void AppendColourQuad(const pddiVector*,const pddiVector2*,const pddiColour*);
 unsigned total=0,allocated=0,indexCount=0;unsigned short* indices=nullptr;int elemState=-1;unsigned draws=0,unlocks=0;
 pddiPrimStream* stream=nullptr;
 void SetQuadIndices(unsigned short*,unsigned);
 void Unlock(pddiPrimStream*){++unlocks;}
 void Display(){++draws;}
};
'''+extract(h,'void SetQuadIndices(unsigned short* idx,unsigned count)').replace('void SetQuadIndices','void Buffer::SetQuadIndices')+r'''
void pddiPrimStream::Vertex(pddiVector* p,pddiColour c,pddiVector2* uv){
 assert(buffer->total<buffer->allocated);Record r;r.position=*p;memcpy(r.uv,uv,8);
 r.colour[0]=c.Red();r.colour[1]=c.Green();r.colour[2]=c.Blue();r.colour[3]=c.Alpha();
 memcpy(buffer->data+buffer->total++*36,&r,36);++calls;
}
pddiPrimStream thePrimStream;
struct pddiBaseContext {unsigned ends=0;void EndPrims(pddiPrimStream*){++ends;}};
struct picaContext:pddiBaseContext {
 alignas(16) unsigned char vertexStorage[PICA_IMM_VERTEX_BYTES]={};
 unsigned short indexStorage[PICA_IMM_INDEX_BYTES/2]={};
 unsigned char* immVertices=vertexStorage;unsigned short* immIndices=indexStorage;
 unsigned immVertexUsed=0,immIndexUsed=0;unsigned short* quadIndices=nullptr;
 unsigned quadCapacity=0,quadWritten=0,quadTotal=0,quadFallbacks=0,quadColourTotal=0;
 Buffer buffer;Buffer* theImmBuffer=&buffer;bool failBegin=false;unsigned begins=0;pddiShader* material=nullptr;
 unsigned char* AllocImmVertices(unsigned);unsigned short* AllocImmIndices(unsigned);
 pddiPrimStream* BeginPrims(pddiShader* shader,int type,int format,int count){
  if(failBegin)return nullptr;assert(type==PDDI_PRIM_TRIANGLES&&format==PDDI_V_CT);
  material=shader;++begins;buffer.data=AllocImmVertices(count);assert(buffer.data);
  buffer.buffer=buffer.data;buffer.coord=(float*)buffer.data;buffer.uv=(float*)(buffer.data+24);buffer.colour=buffer.data+32;
  for(int i=0;i<count;++i){Record r;memcpy(buffer.data+i*36,&r,36);}
  buffer.allocated=count;buffer.total=0;buffer.indices=nullptr;buffer.indexCount=0;
  buffer.stream=&thePrimStream;thePrimStream.buffer=&buffer;return &thePrimStream;
 }
 pddiPrimStream* BeginQuadStream(pddiShader*,unsigned);
 bool EmitQuad(pddiPrimStream*,const pddiVector*,const pddiVector2*,pddiColour,unsigned);
 bool EmitColourQuad(pddiPrimStream*,const pddiVector*,const pddiVector2*,const pddiColour*,unsigned);
 void EndPrims(pddiPrimStream*);void FlushStreams();
};
'''
if direct:
 rewind=s[s.index('    if(coord)',s.index('void picaPrimBuffer::Unlock')):s.index('#ifdef SRR2_OLD3DS_SKIN_RING',s.index('void picaPrimBuffer::Unlock'))]
 prelude=prelude.replace('void Unlock(pddiPrimStream*){++unlocks;}', 'void Unlock(pddiPrimStream*){'+rewind+'++unlocks;}')
tests=r'''
int main(){
 picaContext c;pddiShader shader;uint32_t rng=3927;auto word=[&](){rng=rng*1664525u+1013904223u;return rng;};
 unsigned quads=0;
 const unsigned char order[4][6]={{0,2,1,3,2,0},{0,1,2,0,2,3},{0,1,2,0,3,1},{0,1,2,3,2,1}};
 for(unsigned trial=0;trial<5000;++trial){
  c.immVertexUsed=c.immIndexUsed=0;unsigned count=1+word()%20;
  auto*stream=c.BeginQuadStream(&shader,count);assert(stream&&c.material==&shader);
  Record expected[120];unsigned before=stream->calls;
  for(unsigned q=0;q<count;++q){
   pddiVector points[4];pddiVector2 coords[4];pddiColour colour(word());
   for(unsigned v=0;v<4;++v){for(unsigned k=0;k<3;++k){auto x=word();memcpy(reinterpret_cast<unsigned char*>(points+v)+4*k,&x,4);}for(unsigned k=0;k<2;++k){auto x=word();memcpy(reinterpret_cast<unsigned char*>(coords+v)+4*k,&x,4);}}
   unsigned char snapshot[80];memcpy(snapshot,points,48);memcpy(snapshot+48,coords,32);
   unsigned topology=word()%4;
   pddiColour colours[4]={colour,colour,colour,colour};
#ifdef COLOURED_QUAD
   if(q&1){for(auto& x:colours)x=pddiColour(word());assert(c.EmitColourQuad(stream,points,coords,colours,topology));}
   else
#endif
   assert(c.EmitQuad(stream,points,coords,colour,topology));++quads;
   assert(!memcmp(snapshot,points,48)&&!memcmp(snapshot+48,coords,32));
   for(unsigned j=0;j<6;++j){auto& r=expected[q*6+j];unsigned v=order[topology][j];r.position=points[v];memcpy(r.uv,coords+v,8);r.colour[0]=colours[v].Red();r.colour[1]=colours[v].Green();r.colour[2]=colours[v].Blue();r.colour[3]=colours[v].Alpha();}
  }
  assert(!c.EmitQuad(stream,nullptr,nullptr,pddiColour(0),0));
#ifdef DIRECT_QUAD
  assert((unsigned char*)c.buffer.coord==c.buffer.data+count*4*36);assert((unsigned char*)c.buffer.uv==c.buffer.data+24+count*4*36);assert(c.buffer.colour==c.buffer.data+32+count*4*36);
#endif
  c.EndPrims(stream);assert(!c.quadIndices&&c.buffer.total==count*4&&c.buffer.indexCount==count*6&&c.buffer.elemState==1);
#ifdef DIRECT_QUAD
  assert(stream->calls==before);assert((unsigned char*)c.buffer.coord==c.buffer.data);assert((unsigned char*)c.buffer.uv==c.buffer.data+24);assert(c.buffer.colour==c.buffer.data+32);
#else
  assert(stream->calls-before==count*4);
#endif
  for(unsigned i=0;i<count*6;++i){assert(c.buffer.indices[i]<count*4);assert(!memcmp(c.buffer.data+c.buffer.indices[i]*36,expected+i,36));}
  flushedBytes=flushCount=0;c.FlushStreams();assert(flushCount==2&&flushedBytes==count*(4*36+6*2));
 }
 c.immVertexUsed=c.immIndexUsed=0;c.failBegin=true;assert(!c.BeginQuadStream(&shader,1)&&c.immIndexUsed==0&&c.immVertexUsed==0);c.failBegin=false;
 for(unsigned kind=0;kind<9;++kind){c.immVertexUsed=c.immIndexUsed=0;c.immVertices=c.vertexStorage;c.immIndices=c.indexStorage;g_picaInFrame=true;
  if(kind==0)c.immVertexUsed=PICA_IMM_VERTEX_BYTES-143;if(kind==1)c.immIndexUsed=PICA_IMM_INDEX_BYTES-11;
  if(kind==2)c.immVertices=nullptr;if(kind==3)c.immIndices=nullptr;if(kind==4)g_picaInFrame=false;
  if(kind==6)c.immVertexUsed=PICA_IMM_VERTEX_BYTES+1;if(kind==7)c.immIndexUsed=PICA_IMM_INDEX_BYTES+1;
  unsigned v=c.immVertexUsed,i=c.immIndexUsed,b=c.begins;assert(!c.BeginQuadStream(&shader,kind==5?16385:kind==8?0:1));assert(c.immVertexUsed==v&&c.immIndexUsed==i&&c.begins==b);
 }
 c.immVertexUsed=c.immIndexUsed=0;c.immVertices=c.vertexStorage;c.immIndices=c.indexStorage;g_picaInFrame=true;
 auto*stream=c.BeginQuadStream(&shader,3);pddiVector p[4]={};pddiVector2 uv[4]={};
 assert(!c.EmitQuad(nullptr,p,uv,pddiColour(0),0));assert(!c.EmitQuad(stream,p,uv,pddiColour(0),4));
 assert(c.EmitQuad(stream,p,uv,pddiColour(0),0));c.EndPrims(stream);assert(c.buffer.indexCount==6&&c.buffer.total==4);
 c.immVertexUsed=c.immIndexUsed=0;
 stream=c.BeginQuadStream(&shader,3);assert(stream);c.EndPrims(stream);
 assert(c.buffer.indices && c.buffer.indexCount==0 && c.buffer.total==0);
 unsigned short indices[6];assert(picaQuad::Indices(indices,16383,picaQuad::Particle));assert(indices[4]==65535);assert(!picaQuad::Indices(indices,16384,picaQuad::Particle));
 printf("PASS: %u quads across 5000 production backend streams; exact expanded 36-byte records, all triangle orders, four writes, uint16 bounds, flush spans, reservation fallback, rollback and partial completion\n",quads);
}
'''
stub=r'''
#pragma once
struct pddiVector {float x,y,z;};struct pddiVector2 {float x,y;};
struct pddiColour {unsigned c;explicit pddiColour(unsigned v):c(v){};unsigned Red()const{return(c>>16)&255;}unsigned Green()const{return(c>>8)&255;}unsigned Blue()const{return c&255;}unsigned Alpha()const{return c>>24;}};
struct pddiShader {};struct pddiPrimStream;
'''
with tempfile.TemporaryDirectory(prefix='srr-quads-') as td:
 p=Path(td);(p/'pddi').mkdir();(p/'pddi/pddi.hpp').write_text(stub);(p/'test.cpp').write_text(('#define DIRECT_QUAD\n' if direct else '')+('#define COLOURED_QUAD\n' if coloured else '')+prelude+methods+tests)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(p),'-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
