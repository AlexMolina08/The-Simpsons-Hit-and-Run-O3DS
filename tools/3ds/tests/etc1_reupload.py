#!/usr/bin/env python3
"""Exercise later ETC1 writes through production locking, upload and VRAM ownership."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
namespace = {'__file__': str(Path(__file__).with_name('texture_upload.py'))}
exec(Path(__file__).with_name('texture_upload.py').read_text().split('with tempfile.TemporaryDirectory')[0], namespace)
source, extract, prelude = (namespace[key] for key in ('source', 'extract', 'prelude'))
prelude = prelude.replace('GPU_RGB565, GPU_L8};', 'GPU_RGB565, GPU_L8, GPU_ETC1=12};using GPU_TEXCOLOR=int;')
prelude += '\n#include <pddi/pica/gpuuploadguard.hpp>\nnamespace picaUpload { unsigned gpuHandoff=0; }\n'
prelude = prelude.replace('struct Tex {int width,height,maxLevel; void* data;};',
    'struct Tex {int width,height,maxLevel; void* data;unsigned size=0,fmt=0,param=0;};using C3D_Tex=Tex;using u32=unsigned;')
prelude = prelude.replace('struct picaTexture {', '''
struct picaVramAllocation;
struct pddiRect{};struct pddiLockInfo{int pitch,width,height;void*bits;};
#define PDDIASSERT(x) assert(x)
const int PICA_STAGING_BPP=4;
const unsigned GPU_TEXTURE_ETC1_PARAM=1u<<5;
struct picaTexture {''')
prelude = prelude.replace(' void UploadLevel(int);', '''
 int alphaDepth=8,nMipMap=2;bool created=true;
 unsigned nativeAllocationBytes=0,stagingBytes=0;int stagingLevel=-1;pddiLockInfo lock;
 picaVramAllocation* vramAllocation=nullptr;void* reuploadLinear=nullptr;
 picaVramAllocation* nativeLinearAllocation=nullptr;
 bool vramEligible=true,vramUploaded=false;
 void TryPromoteVram();void FreeStaging();pddiLockInfo* Lock(int,pddiRect*);void Unlock(int);
 void UploadLevel(int);''')
prelude += r'''
#include <cstdint>
using u64=std::uint64_t;static u64 ticks=1;u64 svcGetSystemTick(){return ticks++;}
bool g_picaNativeBundleEnabled=true;
const unsigned SYSCLOCK_ARM11=1000000;namespace NativeBench {bool collecting=false;}
unsigned C3D_TexCalcTotalSize(unsigned size,unsigned levels){return (size-(size>>(2*(levels+1))))*4/3;}
bool failLinear=false,failConversion=false,failOwnership=false;unsigned lastLinearBytes=0;
void* TestMalloc(size_t bytes){return failConversion||failOwnership?nullptr:malloc(bytes);}
void* linearAlloc(unsigned bytes){lastLinearBytes=bytes;return failLinear?nullptr:malloc(bytes);}
void linearFree(void* p){free(p);}
void* vramAlloc(unsigned bytes){return malloc(bytes);}void vramFree(void* p){free(p);}
unsigned vramSpaceFree(){return 8*1024*1024;}bool g_picaInFrame=false;
void C3D_SyncTextureCopy(u32* a,unsigned,u32* b,unsigned,unsigned bytes,unsigned){memcpy(b,a,bytes);}
#define malloc TestMalloc
'''
a = source.index('// Ownership nodes')
b = source.index('\n#endif\n\nstatic const int PICA_STAGING_BPP', a)
methods = source[a:b]
for signature in ('static inline bool IsCompressed', 'static inline GPU_TEXCOLOR PickHardwareFormat',
                  'void picaTexture::FreeStaging', 'pddiLockInfo* picaTexture::Lock',
                  'void picaTexture::Unlock', 'void picaTexture::UploadLevel',
                  'void picaTexture::ConvertAndTile'):
    methods += '\n' + extract(signature)
publish_start = source.index('    const auto publish=[&](bool ok){')
publish_end = source.index('    };', publish_start) + len('    };')
methods += r'''
bool PublishFixture(picaTexture& t,bool ok,bool etc,bool valid,bool hasVram){
 auto& tex=t.tex;auto& hwFormat=t.hwFormat;
 auto& nativeAllocationBytes=t.nativeAllocationBytes;
 auto& nativeLinearAllocation=t.nativeLinearAllocation;
 picaVramAllocation* vramAllocation=hasVram?reinterpret_cast<picaVramAllocation*>(1):nullptr;
 unsigned size=tex.size/4,bytes=C3D_TexCalcTotalSize(size,tex.maxLevel);
''' + source[publish_start:publish_end] + '\n return publish(ok);\n}\n'
tests = r'''
#undef malloc
picaTexture makeTexture(int alpha,int levels,bool etc,bool scale=false){
 int bpp;int format=PickHardwareFormat(alpha,&bpp);
 picaTexture t{};t.xSize=t.ySize=scale?256:128;t.type=PDDI_TEXTYPE_RGB;
 t.hwFormat=etc?GPU_ETC1:format;t.alphaDepth=alpha;t.nMipMap=levels;
 t.tex.width=t.tex.height=128;t.tex.maxLevel=levels;t.tex.size=etc?8192:32768;
 t.tex.fmt=t.hwFormat;t.tex.param=0x45002|(etc?GPU_TEXTURE_ETC1_PARAM:0);
 t.nativeAllocationBytes=C3D_TexCalcTotalSize(32768,levels);
 t.tex.data=malloc(t.nativeAllocationBytes);assert(t.tex.data);
 memset(t.tex.data,0xa5,t.nativeAllocationBytes);t.valid=etc;return t;
}
void dispose(picaTexture& t){
 t.FreeStaging();if(t.reuploadLinear)linearFree(t.reuploadLinear);
 if(t.vramAllocation)RetireVram(t.vramAllocation);
 else if(t.nativeLinearAllocation)RetireVram(t.nativeLinearAllocation);
 else linearFree(t.tex.data);
 picaReclaimVramTextures();
}
int main(){
 pixelBytes=2;std::mt19937 random(33017);
 for(bool linearCompact:{false,true})for(bool compact:{false,true})for(int alpha:{0,1,8})for(int levels:{0,2})for(bool vram:{false,true}){
  g_picaVramCompact=compact;auto t=makeTexture(alpha,levels,true);auto reference=makeTexture(alpha,levels,false);
  reference.vramEligible=false;
  g_picaLinearCompact=linearCompact;
  const unsigned nativeBytes=C3D_TexCalcTotalSize(t.tex.size,levels);
  assert(CompactNativeLinear(t.tex.data,t.nativeAllocationBytes,nativeBytes,t.nativeLinearAllocation)==linearCompact);
  if(linearCompact){assert(t.nativeLinearAllocation && t.nativeLinearAllocation->linear);
   assert(t.nativeLinearAllocation->bytes==nativeBytes);
   for(unsigned i=0;i<nativeBytes;++i)assert(((unsigned char*)t.tex.data)[i]==0xa5);
  }
  if(vram){t.TryPromoteVram();assert(t.vramAllocation);
   assert(t.vramAllocation->bytes==(compact?C3D_TexCalcTotalSize(t.tex.size,levels):t.nativeAllocationBytes));
   for(unsigned i=0;i<C3D_TexCalcTotalSize(t.tex.size,levels);++i)assert(((unsigned char*)t.tex.data)[i]==0xa5);
  }else t.vramEligible=false;
  void* previous=t.tex.data;auto* owner=t.vramAllocation;
  if(vram || linearCompact){
   failLinear=true;assert(!t.Lock(0,nullptr));failLinear=false;
   assert(t.valid&&t.tex.data==previous&&t.vramAllocation==owner&&!t.reuploadLinear);
   assert(t.tex.fmt==GPU_ETC1&&t.tex.size==8192&&(t.tex.param&GPU_TEXTURE_ETC1_PARAM));
  }
  auto* input=t.Lock(0,nullptr);assert(input);
  if(vram || linearCompact)assert(lastLinearBytes==t.nativeAllocationBytes);
  auto* expected=reference.Lock(0,nullptr);assert(expected);
  for(unsigned i=0;i<128*128;++i){unsigned value=random();((unsigned*)input->bits)[i]=value;((unsigned*)expected->bits)[i]=value;}
  t.Unlock(0);reference.Unlock(0);
  assert(t.valid&&t.hwFormat==reference.hwFormat&&t.tex.fmt==reference.hwFormat);
  assert(t.tex.size==32768&&t.tex.maxLevel==levels&&t.tex.param==0x45002);
  assert(!memcmp(t.tex.data,reference.tex.data,t.nativeAllocationBytes));
  assert(!t.vramAllocation&&!t.nativeLinearAllocation&&!t.reuploadLinear);
  if(vram || linearCompact){
   assert(retiredVram&&retiredVram->data==previous);
   for(unsigned i=0;i<nativeBytes;++i)assert(static_cast<unsigned char*>(retiredVram->data)[i]==0xa5);
  }
  dispose(t);dispose(reference);assert(!vramTextureBytes&&!linearCompactLive);
 }
 for(bool vram:{false,true}){
  auto t=makeTexture(8,2,true,true);
  g_picaLinearCompact=true;
  assert(CompactNativeLinear(t.tex.data,t.nativeAllocationBytes,C3D_TexCalcTotalSize(t.tex.size,2),t.nativeLinearAllocation));
  if(vram)t.TryPromoteVram();else t.vramEligible=false;
  void* previous=t.tex.data;auto* owner=t.vramAllocation;auto* linearOwner=t.nativeLinearAllocation;assert(t.Lock(0,nullptr));
  failConversion=true;t.Unlock(0);failConversion=false;
  assert(t.valid&&t.tex.data==previous&&t.vramAllocation==owner&&t.nativeLinearAllocation==linearOwner&&!t.reuploadLinear);
  assert(t.hwFormat==GPU_ETC1&&t.tex.fmt==GPU_ETC1&&t.tex.size==8192&&t.tex.param==0x45022);
  dispose(t);assert(!vramTextureBytes&&!linearCompactLive);
 }
 auto failed=makeTexture(0,0,true);void* original=failed.tex.data;
 failOwnership=true;assert(!CompactNativeLinear(failed.tex.data,failed.nativeAllocationBytes,8192,failed.nativeLinearAllocation));failOwnership=false;
 assert(failed.tex.data==original&&!failed.nativeLinearAllocation);
 failLinear=true;assert(!CompactNativeLinear(failed.tex.data,failed.nativeAllocationBytes,8192,failed.nativeLinearAllocation));failLinear=false;
 assert(failed.tex.data==original&&!failed.nativeLinearAllocation);
 assert(!CompactNativeLinear(failed.tex.data,8192,8192,failed.nativeLinearAllocation));
 dispose(failed);assert(!vramTextureBytes&&!linearCompactLive);
 for(bool ok:{false,true})for(bool etc:{false,true})for(bool valid:{false,true})for(bool hasVram:{false,true}){
  auto first=makeTexture(0,2,false);first.vramEligible=false;void* before=first.tex.data;
  assert(PublishFixture(first,ok,etc,valid,hasVram)==ok);
  const bool compacted=ok&&etc&&!valid&&!hasVram;
  assert(bool(first.nativeLinearAllocation)==compacted);
  assert((first.tex.data!=before)==compacted);
  if(!ok || !etc)assert(first.hwFormat==GPU_RGB565&&first.tex.size==32768);
  dispose(first);assert(!linearCompactLive);
 }
 puts("PASS: ETC1 full reupload matches legacy texels/mips for three formats, linear/VRAM; allocation and conversion failures preserve previous descriptor/ownership.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-etc1-reupload-') as directory:
    folder = Path(directory)
    (folder / 'test.cpp').write_text(prelude + methods + tests)
    for feature in ('SRR2_OLD3DS_NATIVE_BENCH','SRR2_OLD3DS_WORLD_RESIDENCY'):
        subprocess.run(['clang++', '-std=c++14', '-O2', '-fsanitize=address,undefined',
                        '-DSRR2_OLD3DS_NATIVE_ETC1', '-DSRR2_OLD3DS_VRAM_TEXTURES','-D'+feature,
                        '-I' + str(root), '-I' + str(root / 'libs/pure3d'), str(folder / 'test.cpp'),
                        str(root / 'libs/pure3d/pddi/pica/swizzle.cpp'),
                        str(root / 'libs/pure3d/pddi/gl/decompress.cpp'), '-o', str(folder / 'test')], check=True)
        subprocess.run([str(folder / 'test')], check=True)
