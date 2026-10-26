#!/usr/bin/env python3
"""Check lossless rigid vertex storage and the production buffer layout selection."""
from pathlib import Path
import importlib.util, subprocess, tempfile, sys
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
a=source.index('    if(!g_picaState.bufInfoSet)',source.index('void picaPrimBuffer::Display('));b=source.index('{',a);e=b+1;depth=1
while depth:
 depth+=(source[e]=='{')-(source[e]=='}');e+=1
block=source[a:e]
assert 'AttrInfo_AddFixed(&skinRigidAttrInfo, 4);' in source
assert 'C3D_FixedAttribSet(4, 0.0f, 0.0f, 0.0f, 0.0f);' in source
assert 'AttrInfo_AddLoader(&skinRigidAttrInfo, 5, GPU_BYTE, 4);' in source
harness=r'''
#define SRR2_OLD3DS_GPU_SKIN_BORROW
#define SRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET
#include <pddi/pica/skincore.hpp>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#define SRR2_OLD3DS_GPU_SKIN_SUPPORT
#define SRR2_OLD3DS_GPU_SKIN_RIGID_PACK
const unsigned PICA_VERTEX_STRIDE=36;
struct {bool bufInfoSet=false;} g_picaState;
struct C3D_BufInfo{};
unsigned stride,attributes,permutation,updates;
void BufInfo_Init(C3D_BufInfo*){}
void BufInfo_Add(C3D_BufInfo*,const void*,unsigned s,unsigned a,unsigned p){stride=s;attributes=a;permutation=p;}
void C3D_SetBufInfo(C3D_BufInfo*){}
void C3Di_UpdateContext(){++updates;}
struct Prim {bool skinDrawing=false,skinRigid=false;
void Display(){const unsigned char* vertexData=nullptr;
'''+block+r'''
}};
int main(){
 uint32_t random=9377;auto next=[&](){random=random*1664525u+1013904223u;return random;};
 for(unsigned n=0;n<10000;++n){
  picaSkin::Source source={};source.weights[0]=1;source.indices[0]=n%24;
  unsigned char original[36];for(auto& v:original)v=next();
  for(unsigned i=0;i<3;++i){source.position[i]=float(int(next()%20000)-10000)/100;
   source.normal[i]=float(int(next()%2000)-1000)/1000;}
  picaSkin::Vertex full;assert(picaSkin::PackVertex(full,source,original,24));
  unsigned char destination[64];memset(destination,0x5a,sizeof(destination));
  picaSkin::StoreVertex(destination,full,true);
  assert(!memcmp(destination,&full,36));
  picaSkin::RigidVertex rigid;memcpy(&rigid,destination,sizeof(rigid));
  assert(!memcmp(rigid.offsets,full.offsets,4)&&rigid.offsets[3]==-1);
  for(unsigned i=40;i<64;++i)assert(destination[i]==0x5a);
  picaSkin::StoreVertex(destination,full,false);assert(!memcmp(destination,&full,56));
 }
 assert(picaSkin::PackedStride(true)==40&&picaSkin::PackedStride(false)==56);
 Prim p;
 for(unsigned mode=0;mode<3;++mode){p.skinDrawing=mode!=0;p.skinRigid=mode==1;g_picaState.bufInfoSet=false;p.Display();
  assert(stride==(mode==0?36:mode==1?40:56));assert(attributes==(mode==0?4:mode==1?5:6));assert(permutation==(mode==0?0x3210:mode==1?0x53210:0x543210));
  unsigned before=updates;p.Display();assert(updates==before);}
 // Exact pressure snapshot from the physical male2_h_merged rejection.
 const unsigned recordedHeader=14060-501*28;
 assert(recordedHeader==32);
 assert(!picaSkin::FitsBorrowed(14060,149876,251152,3308288));
 assert(picaSkin::FitsBorrowed(501*24+recordedHeader,149876,251152,3308288));
 puts("PASS: 10,000 lossless rigid vertex records; exact offsets/colors/UVs; production 36/40/56-byte buffer selection");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-rigid-skin-') as td:
 p=Path(td);(p/'test.cpp').write_text(harness)
 subprocess.run(['clang++','-std=c++14','-DSRR2_OLD3DS_GPU_SKIN_COMPACT','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
assets=root/'out/whole-frame-1/emulator/sdmc/3ds/simpsons/art/chars'
if assets.exists():
 spec=importlib.util.spec_from_file_location('inventory',root/'tools/3ds/skin_inventory.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 for name in ['homer','bart','male3']:
  record=next(r for r in m.skins(assets/(name+'_m.p3d')) if r['skin']==name+'_h_merged')
  groups=[g for g in record['groups'] if g['bones']<=24]
  previous=sum(32+g['vertices']*56 for g in groups)
  packed=sum(32+g['vertices']*(40 if g['rigid_vertices']==g['vertices'] else 56) for g in groups)
  assert packed<previous
  if name=='male3':assert 2*packed<=65536<2*previous
  print(f'{name}: eligible GPU data {previous} -> {packed} bytes; asset calculation, not frame timing')

subprocess.run([sys.executable,str(root/'tools/3ds/tests/gpu_skin_shader.py'),'--compact','--zero-rigid-weights'],check=True)
