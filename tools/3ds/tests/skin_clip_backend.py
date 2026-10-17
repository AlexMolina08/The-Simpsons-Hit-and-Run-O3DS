#!/usr/bin/env python3
"""Exercise production skin metadata preparation, culling and mutation lifetime."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
def extract(signature):
 start=source.index(signature);opening=source.index('{',start);end=opening+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
prelude=r'''
#include <pddi/pica/skinclip.hpp>
#include <pddi/pica/clipbounds.hpp>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>
using u64=uint64_t;
static u64 svcGetSystemTick(){static u64 tick=0;return ++tick;}
static u64 skinClipTicks;static unsigned skinClipCalls,skinClipSamples;
#define SRR2_OLD3DS_SKIN_CLIP
static unsigned skinClipBytes,skinClipTested,skinClipRejected;
struct {unsigned mutations;} g_picaSkinMetrics;
bool g_picaInFrame=true;
const unsigned PDDI_V_NORMAL=16;
struct pddiMatrix {float data[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};const float* Row4(unsigned r)const{return data[r];}};
bool PrimIsDrawable(unsigned p){return p==1;}
const int PDDI_PROJECTION_PERSPECTIVE=1;
struct Display {bool stereo=false;bool StereoActive()const{return stereo;}};
struct picaContext {
 bool bottomPass=false;Display d;Display* display=&d;
 struct View {int projectionMode=1;} view;struct State {View* viewState;} state={&view};
 bool CullSkinBounds(const picaSkinClip::Bounds*const*,unsigned,const pddiMatrix*,unsigned);
 bool CullBounds(const float low[3],const float high[3]) {
  float m[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
  return picaClipBounds::Reject(picaClipBounds::Prepare(m,m),low,high);
 }
};
struct picaPrimBuffer {
 picaContext* context;bool transient=false;unsigned char buffer[72]={};unsigned allocated=2,primType=1,vertexType=16;
 picaSkinClip::Bounds* skinClipBounds=nullptr;
 picaSkin::Reason skinReason=picaSkin::NotSelected;unsigned skinBoneCount=0;
 bool IndicesInRange(){return true;}
 void PrepareGpuSkin(const void*,unsigned,unsigned,bool,bool=false,bool=false);
 void MutateGpuSkin();
 ~picaPrimBuffer();
};
'''
methods=extract('struct picaSkinClipTimer')+';\n'+'\n'.join(extract(s) for s in ['void picaPrimBuffer::PrepareGpuSkin(', 'void picaPrimBuffer::MutateGpuSkin()', 'bool picaContext::CullSkinBounds('])
# Compile the actual CPU-owned metadata release prefix from the destructor;
# GPU/native-buffer retirement is independently covered by existing suites.
destructor=extract('picaPrimBuffer::~picaPrimBuffer()')
methods+='\n'+destructor[:destructor.index('#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT')]+'}\n'
tests=r'''
int main(){
 picaContext c;pddiMatrix bone;
 picaSkin::Source source[2]={};for(auto& v:source){v.weights[0]=1;v.position[0]=5;}
 {
  picaPrimBuffer b;b.context=&c;b.PrepareGpuSkin(source,2,1,false);assert(b.skinClipBounds&&skinClipBytes==sizeof(picaSkinClip::Bounds));
  const picaSkinClip::Bounds* sets[]={b.skinClipBounds,b.skinClipBounds};
  assert(c.CullSkinBounds(sets,2,&bone,1));assert(skinClipTested==1&&skinClipRejected==1);
  g_picaInFrame=false;assert(!c.CullSkinBounds(sets,2,&bone,1));g_picaInFrame=true;
  c.bottomPass=true;assert(!c.CullSkinBounds(sets,2,&bone,1));c.bottomPass=false;
  c.d.stereo=true;assert(!c.CullSkinBounds(sets,2,&bone,1));c.d.stereo=false;
  assert(!c.CullSkinBounds(nullptr,2,&bone,1));assert(!c.CullSkinBounds(sets,2,&bone,0));
  bone.data[3][0]=-5;assert(!c.CullSkinBounds(sets,2,&bone,1));bone.data[3][0]=0;
  b.MutateGpuSkin();sets[0]=nullptr;assert(!b.skinClipBounds&&!skinClipBytes&&!c.CullSkinBounds(sets,2,&bone,1));
  b.PrepareGpuSkin(source,2,1,false);assert(b.skinClipBounds);
 }
 assert(!skinClipBytes);
 skinClipBytes=picaSkinClip::Budget;{picaPrimBuffer b;b.context=&c;b.PrepareGpuSkin(source,2,1,false);assert(!b.skinClipBounds);}skinClipBytes=0;
 {picaPrimBuffer b;b.context=&c;source[0].weights[0]=-.1f;b.PrepareGpuSkin(source,2,1,false);assert(!b.skinClipBounds);}
 puts("PASS: shared production skin culling, target guards, palette changes, source mutation, allocation-budget fallback and destructor release");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-skin-clip-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+methods+tests)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-Wno-pointer-bool-conversion','-Wno-unused-parameter','-fsanitize=address,undefined','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
