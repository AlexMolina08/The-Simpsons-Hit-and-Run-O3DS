#!/usr/bin/env python3
"""Test production raw VRAM ownership, upload rollback and fenced retirement."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/pddi/pica/texture.cpp').read_text()
def extract(signature):
 a=source.index(signature);b=source.index('{',a);depth=1;i=b+1
 while depth:depth+=(source[i]=='{')-(source[i]=='}');i+=1
 return source[a:i]
a=source.index('// Ownership nodes');b=source.index('\n#endif\n\nstatic const int PICA_STAGING_BPP',a)
prelude=r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <pddi/pica/gpuuploadguard.hpp>
namespace picaUpload { unsigned gpuHandoff=0; }
using u32=unsigned;using pddiTextureType=int;
struct C3D_Tex {void*data=nullptr;unsigned size=0;unsigned maxLevel=0;};
unsigned C3D_TexCalcTotalSize(unsigned size,unsigned level){return (size-(size>>(2*(level+1))))*4/3;}
int g_picaInFrame=0;unsigned freeVram=6*1024*1024,copyCount=0,freedVram=0;
bool failNode=false,failVram=false,failLinear=false;
void* TestMalloc(size_t n){return failNode?nullptr:std::malloc(n);}
void* vramAlloc(unsigned n){if(failVram||n>freeVram)return nullptr;freeVram-=n;return std::malloc(n);}
void vramFree(void*p){++freedVram;std::free(p);}
unsigned vramSpaceFree(){return freeVram;}
void* linearAlloc(unsigned n){return failLinear?nullptr:std::malloc(n);}
void linearFree(void*p){std::free(p);}
void C3D_SyncTextureCopy(u32*a,unsigned,u32*b,unsigned,unsigned n,unsigned flags){assert(!g_picaInFrame&&!picaUpload::Blocked()&&flags==8);++copyCount;memcpy(b,a,n);}
struct picaVramAllocation;
struct pddiRect{};struct pddiLockInfo{int pitch,width,height;void*bits;};
enum{PDDI_TEXTYPE_DXT1,PDDI_TEXTYPE_DXT2,PDDI_TEXTYPE_DXT3,PDDI_TEXTYPE_DXT4,PDDI_TEXTYPE_DXT5,PDDI_TEXTYPE_RGB};
#define PDDIASSERT(x) assert(x)
const int PICA_STAGING_BPP=4;
struct picaTexture{
 C3D_Tex tex;picaVramAllocation*vramAllocation=nullptr;void*reuploadLinear=nullptr;
 unsigned char vramEligible=1;bool vramUploaded=false,valid=true,created=true;
 int xSize=128,ySize=128,nMipMap=0,type=PDDI_TEXTYPE_DXT1;
 unsigned char*staging=nullptr;unsigned stagingBytes=0;int stagingLevel=-1;pddiLockInfo lock;
 void TryPromoteVram();void FreeStaging();pddiLockInfo*Lock(int,pddiRect*);
};
#define malloc TestMalloc
'''
tests=r'''
#undef malloc
picaTexture makeTexture(unsigned bytes=16384,unsigned levels=0){picaTexture t;t.tex.size=bytes;t.tex.maxLevel=levels;unsigned n=C3D_TexCalcTotalSize(bytes,levels);t.tex.data=std::malloc(n);for(unsigned i=0;i<n;++i)((unsigned char*)t.tex.data)[i]=(i*59+(i>>3))&255;return t;}
void dispose(picaTexture&t){t.FreeStaging();if(t.vramAllocation){RetireVram(t.vramAllocation);t.vramAllocation=nullptr;}else linearFree(t.tex.data);t.tex.data=nullptr;}
int main(){
 {auto t=makeTexture();void* old=t.tex.data;unsigned copies=copyCount;
  {picaUpload::Handoff handoff;assert(!g_picaInFrame&&picaUpload::Blocked());
   t.TryPromoteVram();assert(t.tex.data==old&&!t.vramAllocation&&copyCount==copies);
  }
  assert(!picaUpload::Blocked());dispose(t);freeVram=6*1024*1024;
 }
 for(unsigned levels=0;levels<4;++levels){auto t=makeTexture(32768,levels);unsigned n=C3D_TexCalcTotalSize(t.tex.size,levels);std::vector<unsigned char> expected((unsigned char*)t.tex.data,(unsigned char*)t.tex.data+n);t.TryPromoteVram();assert(t.vramAllocation&&memcmp(expected.data(),t.tex.data,n)==0&&vramTextureBytes==n);dispose(t);assert(vramTextureBytes==n);picaReclaimVramTextures();assert(vramTextureBytes==0);freeVram=6*1024*1024;}
 for(unsigned failure=0;failure<4;++failure){auto t=makeTexture();void*old=t.tex.data;unsigned copies=copyCount;failNode=failure==0;failVram=failure==1;g_picaInFrame=failure==2;if(failure==3)freeVram=2*1024*1024;t.TryPromoteVram();assert(t.tex.data==old&&!t.vramAllocation&&copyCount==copies);dispose(t);failNode=failVram=false;g_picaInFrame=0;freeVram=6*1024*1024;}
 std::vector<picaTexture> live;for(unsigned i=0;i<17;++i){live.push_back(makeTexture(131072));live.back().TryPromoteVram();assert(bool(live.back().vramAllocation)==(i<16));}assert(vramTextureBytes==2*1024*1024);for(auto&t:live)dispose(t);assert(vramTextureBytes==2*1024*1024);picaReclaimVramTextures();assert(vramTextureBytes==0);freeVram=6*1024*1024;
 for(unsigned mode:{1u,2u}){
  auto big=makeTexture(262144);big.vramEligible=mode;void*old=big.tex.data;
  std::vector<unsigned char> expected((unsigned char*)old,(unsigned char*)old+262144);
  big.TryPromoteVram();
  // The retained policy caps every individual promotion at 128 KiB.
  assert(!big.vramAllocation && big.tex.data==old);
  assert(!memcmp(big.tex.data,expected.data(),262144));
  dispose(big);picaReclaimVramTextures();assert(vramTextureBytes==0);freeVram=6*1024*1024;
 }
 auto t=makeTexture();t.TryPromoteVram();void*previous=t.tex.data;auto*owner=t.vramAllocation;unsigned bytes=vramTextureBytes;
 failLinear=true;assert(t.Lock(0,nullptr)==nullptr&&t.tex.data==previous&&t.vramAllocation==owner&&!t.reuploadLinear);failLinear=false;
 assert(t.Lock(0,nullptr)&&t.reuploadLinear&&t.tex.data==previous);{picaVramUpload upload(t.tex,t.vramAllocation,t.reuploadLinear,t.valid);assert(t.tex.data!=previous);t.valid=false;}
 assert(t.valid&&t.tex.data==previous&&t.vramAllocation==owner&&!t.reuploadLinear&&vramTextureBytes==bytes);
 assert(t.Lock(0,nullptr));void*linear=t.reuploadLinear;{picaVramUpload upload(t.tex,t.vramAllocation,t.reuploadLinear,t.valid);memset(t.tex.data,0x67,t.tex.size);upload.Complete();}
 assert(t.tex.data==linear&&!t.vramAllocation&&!t.reuploadLinear&&vramTextureBytes==bytes);t.FreeStaging();linearFree(t.tex.data);picaReclaimVramTextures();assert(vramTextureBytes==0);
 puts("PASS: byte-exact raw mip copies, 2 MiB live-plus-retired cap, VRAM reserve, allocation/in-frame fallbacks, reupload lock failure, rollback, commit and fenced retirement");
}
'''
code=prelude+source[a:b]+'\n'+extract('static inline bool IsCompressed')+'\n'+extract('void picaTexture::FreeStaging')+'\n'+extract('pddiLockInfo* picaTexture::Lock')+tests
with tempfile.TemporaryDirectory(prefix='srr-vram-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 for visual in (False,True):
  flags=['-DSRR2_OLD3DS_VISUAL_UPGRADE'] if visual else []
  subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Wno-unused-parameter','-fsanitize=address,undefined','-DSRR2_OLD3DS_VRAM_TEXTURES',*flags,'-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
