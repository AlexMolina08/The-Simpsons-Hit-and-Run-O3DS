#!/usr/bin/env python3
"""Compare offline PNG staging with the production PNG/TextureBuilder path."""
import io
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools/3ds'))
from prepare_native_textures import function
from skin_inventory import chunks

PRELUDE = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <png.h>
using P3D_U16=unsigned short;using P3D_U32=unsigned;using PDDI_U32=unsigned;
#define P3DASSERT(x) assert(x)
#define P3DASSERTMSG(x,m,s) assert(x)
struct pddiColour {
 unsigned c=0;
 pddiColour()=default;
 pddiColour(unsigned r,unsigned g,unsigned b,unsigned a):c((a<<24)|(r<<16)|(g<<8)|b){}
 operator unsigned() const{return c;}
 void SetAlpha(unsigned a){c=(c&0xffffff)|(a<<24);}
};
namespace p3d {void* MallocTemp(size_t n){return malloc(n);}void FreeTemp(void* p){free(p);}void print(const char*){}}
struct tFile {FILE* file;enum {BYTE};void GetData(void* p,size_t n,int){assert(fread(p,1,n,file)==n);}};
struct tImageHandler {
 struct Builder {
  enum Origin {TOP,BOTTOM};
  virtual bool BeginImage(int,int,int,Origin,pddiColour*)=0;
  virtual void ProcessScanline32(unsigned*)=0;
  virtual void ProcessScanline8(unsigned char*)=0;
  virtual void EndImage()=0;
 };
};
struct Lock {
 unsigned depth=32,width=0,height=0;bool native=false;
 int rgbaRShift[4]={16,0,0,0},rgbaLShift[4]={0,0,16,0};
 unsigned MakeColour(unsigned);
};
struct TextureBuilder: tImageHandler::Builder {
 void* texture=this;Lock lock,*lockInfo=&lock;unsigned* palette=nullptr;
 unsigned char* scanline=nullptr;int stride=0;unsigned totalScanline=0;
 std::vector<unsigned char> pixels;
 bool BeginImage(int w,int h,int,tImageHandler::Builder::Origin,pddiColour* p) override {
  lock.width=w;lock.height=h;stride=w*4;pixels.resize(w*h*4);scanline=pixels.data();palette=reinterpret_cast<unsigned*>(p);return true;
 }
 void ProcessScanline32(unsigned*) override;void ProcessScanline8(unsigned char*) override;
 void EndImage() override{}
};
struct tPNGHandler {void CreateImage(tFile*,tImageHandler::Builder*);};
static void p3dPngYield(int){}
static void LoadPNG4(png_structp,png_infop,tImageHandler::Builder*);
static void LoadPNG8(png_structp,png_infop,tImageHandler::Builder*);
static void LoadPNG32(png_structp,png_infop,tImageHandler::Builder*);
'''

with tempfile.TemporaryDirectory(prefix='srr-png-staging-') as temporary:
    folder = Path(temporary)
    png = (ROOT / 'libs/pure3d/p3d/png.cpp').read_text()
    factory = (ROOT / 'libs/pure3d/p3d/imagefactory.cpp').read_text()
    pddi = (ROOT / 'libs/pure3d/pddi/pddi.hpp').read_text()
    colour = function(pddi, 'PDDI_U32 MakeColour').replace('PDDI_U32 MakeColour', 'unsigned Lock::MakeColour')
    translation = PRELUDE + colour
    for signature in ('void TextureBuilder::ProcessScanline32', 'void TextureBuilder::ProcessScanline8'):
        translation += function(factory, signature)
    for signature in ('static void pngRead', 'static void p3d_png_err', 'static void p3d_png_warn',
                      'static void * p3d_png_malloc', 'static void p3d_png_free', 'void tPNGHandler::CreateImage',
                      'void LoadPNG4(png_structp pPNG', 'void LoadPNG8(png_structp pPNG', 'void LoadPNG32(png_structp pPNG'):
        translation += function(png, signature)
    translation += r'''
int main(int argc,char** argv){assert(argc==3);FILE* f=fopen(argv[1],"rb");assert(f);
 tFile file{f};TextureBuilder builder;tPNGHandler handler;handler.CreateImage(&file,&builder);fclose(f);
 f=fopen(argv[2],"wb");assert(f);assert(fwrite(builder.pixels.data(),1,builder.pixels.size(),f)==builder.pixels.size());fclose(f);}
'''
    (folder / 'png.cpp').write_text(translation)
    binary = folder / 'png'
    # Use the same bundled libpng source revision as the 3DS build, compiled for
    # host. Reusing an unrelated decoder would weaken the staging comparison.
    build = folder / 'libpng'
    safe_source=folder/'png-source'
    safe_source.symlink_to(ROOT/'out/deps/libpng', target_is_directory=True)
    subprocess.run(['cmake', '-S', str(safe_source), '-B', str(build),
                    '-DPNG_SHARED=OFF', '-DPNG_TESTS=OFF', '-DPNG_TOOLS=OFF',
                    '-DPNG_ARM_NEON=off', '-DCMAKE_BUILD_TYPE=Release'], check=True, stdout=subprocess.DEVNULL)
    subprocess.run(['cmake', '--build', str(build), '-j', '4'], check=True, stdout=subprocess.DEVNULL)
    library = next(build.glob('libpng16.a'))
    subprocess.run(['clang++', '-std=c++14', '-O1', '-g', '-fsanitize=address,undefined',
                    '-I', str(ROOT/'out/deps/libpng'), '-I', str(build), str(folder/'png.cpp'),
                    str(library), '-lz', '-o', str(binary)], check=True)
    images = []
    for asset in sorted((ROOT/'sdcard/3ds/simpsons/art/cars').glob('*.p3d'))[:12]:
        data = asset.read_bytes()
        def walk(start, end):
            for kind, body, child, stop in chunks(data, start, end):
                if kind == 0x19002:
                    size, = struct.unpack_from('<I', data, body)
                    raw = data[body+4:body+4+size]
                    if raw[:8] == b'\x89PNG\r\n\x1a\n':
                        images.append(raw)
                walk(child, stop)
        walk(0, len(data))
    # RGB transparency deliberately stays opaque, matching production rather
    # than Pillow's default conversion of the tRNS chunk.
    for mode in ('RGB', 'RGBA', 'P'):
        image = Image.new(mode, (16, 8))
        encoded = io.BytesIO()
        image.save(encoded, format='PNG', transparency=(0, 0, 0) if mode == 'RGB' else 0 if mode == 'P' else None)
        images.append(encoded.getvalue())
    tested = 0
    for raw in images:
        if raw[28] or raw[24] not in (4, 8) or raw[25] not in (2, 3, 6):
            continue
        source, result = folder/'input.png', folder/'rgba.bin'
        source.write_bytes(raw)
        subprocess.run([str(binary), str(source), str(result)], check=True)
        with Image.open(io.BytesIO(raw)) as image:
            expected = image.convert('RGBA')
            if raw[25] == 2:
                expected.putalpha(255)
            assert result.read_bytes() == expected.tobytes(), f'PNG staging differs, case {tested}'
        tested += 1
    assert tested >= 20
    print(f'PASS: {tested} real/synthetic PNG images match production decode, palette/transparency and TextureBuilder staging byte-for-byte.')
