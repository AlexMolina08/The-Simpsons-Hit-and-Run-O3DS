#!/usr/bin/env python3
"""Exercise production load preparation and unchanged CPU/GPU color transport."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/pure3d/pddi/pica/context.cpp').read_text()
method = source[source.index('void picaPrimBuffer::PrepareGpuSkin('):
                source.index('void picaPrimBuffer::MutateGpuSkin()')]
harness = r'''
#include <pddi/pica/skincore.hpp>
#include <pddi/pica/characterform.hpp>
#include <cassert>
#include <cstring>
#include <limits>
#include <cstdio>
#define SRR2_OLD3DS_CHARACTER_FORM
namespace Old3dsPerformance {static bool flat=true;bool FlatLighting(){return flat;}}
const unsigned PICA_VERTEX_STRIDE=36,PICA_VERTEX_OFF_COL=32,PDDI_V_NORMAL=16;
static unsigned flushes=0;
void GSPGPU_FlushDataCache(void*,unsigned){++flushes;}
void picaSkinPrepareCheckpoint(unsigned){}
struct picaPrimBuffer {
 bool transient=false,expandDirty=false;
 unsigned allocated=1,vertexType=PDDI_V_NORMAL,primType=0,mem=36,skinBoneCount=0;
 unsigned char storage[36],*buffer=storage;
 picaSkin::Reason skinReason=picaSkin::NotSelected;
 bool PrimIsDrawable(unsigned){return true;}bool IndicesInRange(){return true;}
 void PrepareGpuSkin(const void*,unsigned,unsigned,bool,bool,bool,const char* name=nullptr,unsigned group=0);
 picaPrimBuffer(){memset(storage,0xa7,sizeof(storage));memset(storage+32,255,4);}
};
'''
tests = r'''
int main(){
 picaSkin::Source v={};v.weights[0]=1;v.normal[1]=-1;
 picaPrimBuffer baseline;unsigned char original[36];memcpy(original,baseline.storage,36);
 baseline.PrepareGpuSkin(&v,1,1,false,false,false);
 assert(!memcmp(original,baseline.storage,36)&&flushes==0);
 Old3dsPerformance::flat=false;
 baseline.PrepareGpuSkin(&v,1,1,true,false,false);
 assert(!memcmp(original,baseline.storage,36)&&flushes==0);
 Old3dsPerformance::flat=true;
 for(unsigned test=0;test<100000;++test){
  picaPrimBuffer b;
  v.normal[0]=float(int(test%127)-63);v.normal[1]=float(int(test%83)-41);v.normal[2]=17;
  for(unsigned c=0;c<4;++c)b.storage[32+c]=(test*(c+1))%256;
  memcpy(original,b.storage,36);
  // Even an unsupported palette keeps its shading in the CPU fallback buffer.
  b.PrepareGpuSkin(&v,1,26,true,false,false);
  assert(b.skinReason==picaSkin::Palette && b.expandDirty);
  assert(!memcmp(original,b.storage,32)&&original[35]==b.storage[35]);
  for(unsigned c=0;c<3;++c){assert(b.storage[32+c]<=original[32+c]);
   assert(b.storage[32+c]+1>=original[32+c]*.64f);}
  picaSkin::Vertex gpu;
  assert(picaSkin::PackVertex(gpu,v,b.storage,1));
  assert(!memcmp(gpu.colour,b.storage+32,4));
  unsigned char cpu[36]={};
  // LockSkin's existing immutable UV/color copy, followed by position/normal writes.
  memcpy(cpu+24,b.storage+24,12);memcpy(cpu,&gpu,24);
  assert(!memcmp(cpu+32,gpu.colour,4));
 }
 const float invalid[][3]={{0,0,0},{0,std::numeric_limits<float>::infinity(),0},
  {std::numeric_limits<float>::quiet_NaN(),0,0},{1.e-20f,0,0}};
 for(const auto& n:invalid){unsigned char c[]={17,93,255,79},before[4];memcpy(before,c,4);
  assert(!picaCharacterForm::Bake(n,c)&&!memcmp(c,before,4));}
 picaPrimBuffer wrongCount;memcpy(original,wrongCount.storage,36);
 wrongCount.PrepareGpuSkin(&v,0,1,true,false,false);
 assert(!memcmp(original,wrongCount.storage,36));
 // Preparation adds no shader work, vertex attributes, palette bytes or draw calls.
 puts("PASS: 100000 load preparations; CPU palette fallbacks, exact geometry/alpha, bounded RGB, GPU/CPU transport and invalid normals");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-character-form-') as td:
    folder = Path(td)
    (folder / 'test.cpp').write_text(harness + method + tests)
    subprocess.run(['clang++', '-std=c++14', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-fsanitize=address,undefined', '-I' + str(root / 'libs/pure3d'),
                    str(folder / 'test.cpp'), '-o', str(folder / 'test')], check=True)
    subprocess.run([str(folder / 'test')], check=True)
