#!/usr/bin/env python3
"""Compare production uploads byte-for-byte, including block streaming and mips."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[3]
source = (ROOT/'libs/pure3d/pddi/pica/texture.cpp').read_text()
def extract(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]
prelude = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#include "libs/pure3d/pddi/pica/texturetile.hpp"
#include "libs/pure3d/pddi/pica/swizzle.hpp"
#include "libs/pure3d/pddi/gl/decompress.h"
enum {GPU_RGBA8, GPU_RGBA5551, GPU_LA8, GPU_RGBA4, GPU_RGB565};
using pddiTextureType=int;
enum {PDDI_TEXTYPE_RGB, PDDI_TEXTYPE_DXT1, PDDI_TEXTYPE_DXT2, PDDI_TEXTYPE_DXT3, PDDI_TEXTYPE_DXT4, PDDI_TEXTYPE_DXT5};
struct Tex {int width,height,maxLevel; void* data;};
void C3D_TexFlush(Tex*) {}
void* C3D_TexGetImagePtr(Tex* t,void* data,int mip,void*) {
 size_t offset=0;int w=t->width,h=t->height;
 for(int i=0;i<mip;++i){offset+=w*h;w/=2;h/=2;}
 // Tests set the active byte size here before every upload.
 extern unsigned pixelBytes;
 return (char*)data+offset*pixelBytes;
}
unsigned pixelBytes;
static unsigned yields=0;
void Yield(){++yields;}
void (*g_picaYieldHook)()=Yield;
struct picaTexture {
 int xSize,ySize,type,hwFormat;Tex tex;unsigned char* staging;bool valid=false;
 void UploadLevel(int);
 void ConvertAndTile(const unsigned int*,int,int,void*);
};
'''
tests = r'''
int main(int argc,char** argv){
 assert(argc==2);FILE* out=fopen(argv[1],"wb");assert(out);
 std::mt19937 random(492081);
 unsigned cases=0;
 for(int type=PDDI_TEXTYPE_RGB;type<=PDDI_TEXTYPE_DXT5;++type)
 for(int format=GPU_RGBA8;format<=GPU_RGB565;++format)
 for(int w:{8,16,32,128,256,512,1024})
 for(int shape=0;shape<4;++shape){
  int h=shape==0?w:shape==1?8:32;
  int dw=shape==2&&w>8?w/2:w,dh=h;
  pixelBytes=format==GPU_RGBA8?4:2;
  int mips=shape==3&&w>=16?1:0;
  unsigned srcBytes=type==PDDI_TEXTYPE_RGB?w*h*4:((w+3)/4)*((h+3)/4)*(type==PDDI_TEXTYPE_DXT1?8:16);
  std::vector<unsigned> src((srcBytes+3)/4);
  for(auto& p:src)p=random();
  std::vector<unsigned char> bytes((dw*dh+(mips?dw*dh/4:0))*pixelBytes+32,0xa5);
  picaTexture texture{w,h,type,format,{dw,dh,mips,bytes.data()},(unsigned char*)src.data(),false};
  texture.UploadLevel(0);assert(texture.valid);
  for(size_t i=bytes.size()-32;i<bytes.size();++i)assert(bytes[i]==0xa5);
  fwrite(bytes.data(),1,bytes.size(),out);++cases;
 }
 fclose(out);printf("%u production texture uploads passed\n",cases);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-texture-test-') as td:
    temp=Path(td); cpp=temp/'test.cpp'
    cpp.write_text(prelude+extract('static inline bool IsCompressed')+extract('void picaTexture::UploadLevel')+extract('void picaTexture::ConvertAndTile')+tests)
    outputs=[]
    for mode in ['reference','tiles']:
        binary=temp/mode; data=temp/(mode+'.bin'); outputs.append(data)
        flags=[] if mode=='reference' else ['-DSRR2_OLD3DS_TEXTURE_TILES']
        subprocess.run(['clang++','-std=c++14','-O2','-fsanitize=address,undefined',*flags,'-I',str(ROOT),'-I',str(ROOT/'libs/pure3d'),str(cpp),str(ROOT/'libs/pure3d/pddi/pica/swizzle.cpp'),str(ROOT/'libs/pure3d/pddi/gl/decompress.cpp'),'-o',str(binary)],check=True)
        subprocess.run([str(binary),str(data)],check=True)
    assert outputs[0].read_bytes()==outputs[1].read_bytes(), 'Texture upload mismatch'
print('All texels/mips match: five GPU formats, RGB/DXT1/2/3/4/5, flips, scaling, rectangular textures and upload guards.')
