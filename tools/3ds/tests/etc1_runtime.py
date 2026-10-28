#!/usr/bin/env python3
"""Exercise the production native read seam with source-bound ETC1 fixtures."""
from pathlib import Path
import hashlib
import struct
import subprocess
import sys
import tempfile
import zlib

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/3ds'))
from prepare_native_bundle import index_bundle
from prepare_native_textures import function

implementation = (root / 'libs/pure3d/pddi/pica/texture.cpp').read_text()
fragment = function(implementation, 'bool picaTexture::ReadPrepared(')
promotion = function(implementation, 'void picaTexture::TryPromoteVram(')
source = b'world-encoded-image'
payload = bytes([0x55]) * 8192
prefix = struct.pack('<11I', 0x3158544e, 2, 128, 128, 256, 128, 128, 4, 0, len(source), len(payload))
prefix += hashlib.sha256(source).digest()
key = hashlib.sha256(prefix).hexdigest()
encoded = prefix + hashlib.sha256(payload).digest() + payload

prelude = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <CommonCrypto/CommonDigest.h>
#include "pddi/pica/nativetexture.hpp"
#include "pddi/pica/gpuuploadguard.hpp"
namespace picaUpload {unsigned gpuHandoff=0;}
#include "pddi/pica/nativebundle.hpp"
#include "bundle_index.hpp"
using AVSHA=CC_SHA256_CTX;
AVSHA* av_sha_alloc(){return static_cast<AVSHA*>(malloc(sizeof(AVSHA)));}
int av_sha_init(AVSHA* c,int bits){assert(bits==256);return CC_SHA256_Init(c)?0:-1;}
void av_sha_update(AVSHA* c,const unsigned char* p,size_t n){CC_SHA256_Update(c,p,n);}
void av_sha_final(AVSHA* c,unsigned char* p){CC_SHA256_Final(p,c);}
void av_free(void* p){free(p);}
bool g_picaNativeEtcEnabled=true,g_picaNativeTexturesEnabled=true;
bool g_picaNativeBundleEnabled=true,g_picaNativeTextureDiagnostics=false;
unsigned nativeTextureHits=0,nativeTextureMisses=0,nativeTextureInvalid=0,nativeTextureBytes=0;
void NativeTextureMiss(const unsigned char*){++nativeTextureMisses;}
bool picaNativeTexturesAvailable(){return g_picaNativeTexturesEnabled;}
void (*g_picaYieldHook)()=nullptr;
struct NativeTextureCost {
 enum Bucket {Other,HashWork,Open,ReadCheck};unsigned outcome=2;
 explicit NativeTextureCost(bool){}
 unsigned Select(unsigned){return Other;}
 struct Phase{Phase(NativeTextureCost&,unsigned){}};
};
struct NativeBundleFile {
 FILE* file=fopen(picaNativeBundle::BundlePath,"rb");
 ~NativeBundleFile(){if(file)fclose(file);}
};
enum {GPU_RGB565=1,GPU_RGBA5551=2,GPU_RGBA4=3,GPU_ETC1=12};
constexpr unsigned GPU_TEXTURE_ETC1_PARAM=1u<<5;
unsigned C3D_TexCalcTotalSize(unsigned size,unsigned levels){return (size-(size>>(2*(levels+1))))*4/3;}
using u32=unsigned;
struct picaVramAllocation {void* data;unsigned bytes;picaVramAllocation* next;};
unsigned vramTextureBytes=0,vramTexturePeak=0,vramTextureCopies=0,vramTextureEligible=0;
unsigned vramTextureInFrame=0,vramTextureFailures=0,copiedBytes=0,allocatedVram=0;
bool g_picaInFrame=false;
void* freedLinear=nullptr;
unsigned vramSpaceFree(){return 8*1024*1024;}
void* vramAlloc(unsigned bytes){allocatedVram=bytes;return malloc(bytes);}
void linearFree(void* data){freedLinear=data;}
void C3D_SyncTextureCopy(u32* source,unsigned,u32* target,unsigned,unsigned bytes,unsigned){
 copiedBytes=bytes;memcpy(target,source,bytes);
}
struct picaTexture {
 unsigned hwFormat=GPU_RGBA4,xSize=128,ySize=128,nativeAllocationBytes=32768;
 struct Tex {unsigned size=32768,maxLevel=0,width=128,height=128,fmt=GPU_RGBA4,param=0x45002;void* data=nullptr;} tex;
 bool ReadPrepared(const void*,unsigned,unsigned,bool world=false);
 bool vramUploaded=false,vramEligible=true;picaVramAllocation* vramAllocation=nullptr;
 void TryPromoteVram();
};
'''
test = r'''
int main(){
 using picaNativeTexture::WorldSource;
 for(const char* p:{"art/l2r1.p3d","sdmc:/3ds/simpsons/art/L1_TERRA.p3d","art\\L2Z1.P3D"})assert(WorldSource(p));
 for(const char* p:{"art/l02_fx.p3d","art/cars/l2r1.p3d","art/frontend/license.p3d","art/cards.p3d","l2r1.p3d"})assert(!WorldSource(p));
 std::vector<unsigned char> memory(32768+32,0xa5);
 picaTexture texture;texture.tex.data=memory.data();
 const char source[]="world-encoded-image";
 assert(!texture.ReadPrepared(source,sizeof(source)-1,256,false));
 assert(nativeTextureHits==0 && nativeTextureMisses==0 && texture.tex.size==32768);
 for(auto byte:memory)assert(byte==0xa5);
 assert(!texture.ReadPrepared("different-source",16,256,true));
 assert(nativeTextureMisses==1 && texture.tex.size==32768 && texture.hwFormat==GPU_RGBA4);
 assert(texture.ReadPrepared(source,sizeof(source)-1,256,true));
 assert(nativeTextureHits==1 && nativeTextureBytes==8192 && nativeTextureInvalid==0);
 assert(texture.tex.size==8192 && texture.tex.fmt==GPU_ETC1 && texture.hwFormat==GPU_ETC1);
 assert(texture.nativeAllocationBytes==32768 && texture.tex.maxLevel==0);
 assert(texture.tex.param==(0x45002|GPU_TEXTURE_ETC1_PARAM));
 for(unsigned i=0;i<8192;++i)assert(memory[i]==0x55);
 for(unsigned i=8192;i<memory.size();++i)assert(memory[i]==0xa5);
 texture.TryPromoteVram();
 assert(allocatedVram==32768 && copiedBytes==8192 && vramTextureBytes==32768);
 assert(freedLinear==memory.data() && texture.vramAllocation->bytes==32768);
 assert(texture.tex.data==texture.vramAllocation->data && !memcmp(texture.tex.data,memory.data(),8192));
 FILE* f=fopen(picaNativeBundle::BundlePath,"r+b");assert(f);
 assert(!fseek(f,84+108+13,SEEK_SET));assert(fputc(0x54,f)!=EOF);fclose(f);
 picaTexture corrupt;corrupt.tex.data=memory.data();
 assert(!corrupt.ReadPrepared(source,sizeof(source)-1,256,true));
 assert(nativeTextureInvalid==1 && corrupt.tex.size==32768 && corrupt.hwFormat==GPU_RGBA4);
 assert(corrupt.tex.fmt==GPU_RGBA4 && corrupt.nativeAllocationBytes==32768);
 assert(corrupt.tex.param==0x45002);
 g_picaNativeTexturesEnabled=false;
 assert(!corrupt.ReadPrepared(source,sizeof(source)-1,256,true));
 assert(nativeTextureInvalid==1);
 free(texture.vramAllocation->data);free(texture.vramAllocation);
 puts("PASS: world eligibility, checked publication, original allocation/mips and VRAM budgets, compressed copy bounds, source/corruption fallback and destination guards.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-etc1-runtime-') as directory:
    folder = Path(directory)
    bundle = folder / 'native146.bundle'
    bundle.write_bytes(struct.pack('<II', 0x314e4233, 1) + (key + '.ntx').encode() +
                       struct.pack('<II', len(encoded), zlib.crc32(encoded)) + encoded)
    index_bundle(bundle, folder / 'bundle_index.hpp', 16, 'native146.bundle', True)
    cpp = folder / 'test.cpp'
    cpp.write_text(prelude + fragment + promotion + test)
    subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined', '-Wno-deprecated-declarations',
                    '-DSRR2_OLD3DS_NATIVE_ETC1', '-DSRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE',
                    '-I' + str(root / 'libs/pure3d'), '-I' + str(folder), str(cpp), '-o', str(folder / 'test')], check=True)
    subprocess.run([str(folder / 'test')], cwd=folder, check=True)
