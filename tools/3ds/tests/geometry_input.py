#!/usr/bin/env python3
"""Compare production geometry attribute dispatch with bounded reads ON/OFF."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/pure3d/p3d/primgroup.cpp').read_text()
source = source[source.index('tPrimGroup*  tPrimGroupLoader::Load(tChunkFile'):]
cases = []
for name in ('POSITIONLIST', 'NORMALLIST', 'COLOURLIST', 'UVLIST'):
    start = re.search(r'case Pure3D::Mesh::' + name + r':\s*\{', source).start()
    opening = source.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    cases.append(source[start:end] + 'break;')
prelude = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <random>
#include "p3d/geometryinput.hpp"
namespace p3dGeometryInput {bool enabled=true;unsigned records=0,reads=0,bytes=0;}
unsigned yieldCalls=0;
extern "C" void p3dYieldIfDue(){++yieldCalls;}
namespace Pure3D {namespace Mesh {enum {POSITIONLIST,NORMALLIST,COLOURLIST,UVLIST};}}
namespace rmt {struct Vector {float x,y,z;};}
using pddiVector=rmt::Vector;
struct pddiVector2 {float u,v;};struct pddiColour {unsigned word;};
enum {PDDI_V_NORMAL=16,PDDI_V_COLOUR=32};
#define P3DASSERT(x) assert(x)
struct tFile {enum DataType {DWORD=4};};
struct tChunkFile {
 std::vector<unsigned> data;unsigned position=0,calls=0,maxWords=0;
 int GetLong(){assert(position<data.size());return int(data[position++]);}
 bool GetData(void* out,unsigned count,tFile::DataType type){
  assert(type==tFile::DWORD&&position+count<=data.size());++calls;
  if(count>maxWords)maxWords=count;
  memcpy(out,data.data()+position,count*4);position+=count;return true;
 }
};
struct Stream {
 std::vector<unsigned> trace;
 void Value(float value){unsigned bits;memcpy(&bits,&value,4);trace.push_back(bits);}
 void Coord(float x,float y,float z){trace.push_back(1);Value(x);Value(y);Value(z);}
 void Normal(float x,float y,float z){trace.push_back(2);Value(x);Value(y);Value(z);}
 void Colour(pddiColour value){trace.push_back(3);trace.push_back(value.word);}
 void UV(float u,float v,int channel){trace.push_back(4);Value(u);Value(v);trace.push_back(channel);}
 void Next(){trace.push_back(5);}
};
void Load(unsigned attribute,tChunkFile* f,Stream* stream,void* bones,int count,unsigned format,
          int nUVChannel,rmt::Vector* tmpPositions){
 bool primBufferInitialized=true;int mVertexCount=count;unsigned mVertexFormat=format;
 switch(attribute){
'''
tests = r'''
}}
int main(){
 std::mt19937 random(33018);unsigned trials=0;
 for(unsigned attribute=0;attribute<4;++attribute)
 for(int count:{-1,0,1,2,63,64,65,257,1024})
 for(bool bones:{false,true})for(bool disabled:{false,true})for(int channel:{0,1,2}){
  unsigned words=attribute<2?3:attribute==2?1:2;
  tChunkFile original;original.data.push_back(unsigned(count));
  if(attribute==3)original.data.push_back(channel);
  for(int i=0;i<count*int(words);++i)original.data.push_back(random());
  Stream output[2];tChunkFile reader[2]={original,original};std::vector<unsigned char> position[2];
  for(unsigned mode=0;mode<2;++mode){
   position[mode].assign((count>0?unsigned(count)*sizeof(rmt::Vector):0)+32,0xa5);
   p3dGeometryInput::enabled=mode;p3dGeometryInput::records=p3dGeometryInput::reads=p3dGeometryInput::bytes=0;yieldCalls=0;
   Load(attribute,&reader[mode],&output[mode],bones?&reader[mode]:nullptr,count,
        disabled?0:PDDI_V_NORMAL|PDDI_V_COLOUR,2,(rmt::Vector*)position[mode].data());
   bool hasInput=count>0&&!(disabled&&(attribute==1||attribute==2))&&!(attribute==3&&channel>=2);
   bool eligible=hasInput&&!bones;
#ifndef SRR2_OLD3DS_GEOMETRY_INPUT
   eligible=false;
#endif
   assert(p3dGeometryInput::records==(mode&&eligible?unsigned(count):0));
   assert(p3dGeometryInput::reads==(mode&&eligible?(unsigned(count)+63)/64:0));
   assert(yieldCalls==p3dGeometryInput::reads);
   assert(p3dGeometryInput::bytes==(mode&&eligible?unsigned(count)*words*4:0));
   assert(reader[mode].calls==(mode&&eligible?(unsigned(count)+63)/64:hasInput?unsigned(count):0));
   if(mode&&eligible)assert(reader[mode].maxWords<=64*words);
  }
  assert(output[0].trace==output[1].trace&&position[0]==position[1]);
  assert(reader[0].position==reader[1].position);
  for(unsigned i=position[1].size()-32;i<position[1].size();++i)assert(position[1][i]==0xa5);
  ++trials;
 }
 printf("PASS: %u production attribute cases; exact values, stream order, deformation positions, bone/format/channel fallbacks, input boundaries and <=768-byte work units.\n",trials);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-geometry-input-') as directory:
    folder = Path(directory)
    (folder / 'test.cpp').write_text(prelude + '\n'.join(cases) + tests)
    for flags in ([], ['-DSRR2_OLD3DS_GEOMETRY_INPUT']):
        subprocess.run(['clang++', '-std=c++14', '-O2', '-fsanitize=address,undefined',
                        *flags, '-I' + str(root / 'libs/pure3d'),
                        str(folder / 'test.cpp'), '-o', str(folder / 'test')], check=True)
        subprocess.run([str(folder / 'test')], check=True)
