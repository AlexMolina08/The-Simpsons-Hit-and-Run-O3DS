#!/usr/bin/env python3
"""The physical image must include both edges of 400 and 800 column displays."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/main/worldstreaming.cpp').read_text()
function = source[source.index('static void CaptureImage('):source.index('\nvoid MarkerImage()')]
code = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using u64=uint64_t;
#define R_FAILED(value) ((value)<0)
constexpr unsigned GSP_BGR8_OES=1;
// Match libctru: the address is u32*, even when its pixels are three bytes.
struct Buffer {uint32_t* framebuf0_vaddr;unsigned format,framebuf_widthbytesize;};
struct GSPGPU_CaptureInfo {Buffer screencapture[1];};
static unsigned width=400,route=1000,nextImage=0,triggerSequence=0,imageSequence=0,imageCapacity=1,imageFailures=0;
static float progress=0;
static unsigned imageImportFailures=0,imageLayoutFailures=0,imageCacheFailures=0,imageChangedFailures=0;static uint64_t imageCopyMaxTicks=0;
static std::vector<unsigned char> source;
struct Image {u64 tick;unsigned route,sequence,reason;float distance;unsigned char pixels[400*240*3];};
static Image storage;static Image* images=&storage;
namespace Graphics3DS {unsigned RenderWidth(){return width;}}
int GSPGPU_ImportDisplayCaptureInfo(GSPGPU_CaptureInfo* output){output->screencapture[0]={reinterpret_cast<uint32_t*>(source.data()),GSP_BGR8_OES,720};return 0;}
int GSPGPU_InvalidateDataCache(uint32_t* p,unsigned bytes){assert(reinterpret_cast<unsigned char*>(p)==source.data()&&bytes<=source.size());return 0;}
u64 svcGetSystemTick(){return 123;}
''' + function + r'''
int main(){
 for(unsigned columns:{400u,800u}){
  width=columns;source.resize(width*240*3);nextImage=0;
  for(unsigned x=0;x<width;++x)for(unsigned i=0;i<720;++i)source[x*720+i]=(x*17+i)%251;
  CaptureImage(3);assert(!imageFailures&&storage.sequence&&storage.reason==3);
  for(unsigned x=0;x<400;++x)for(unsigned i=0;i<720;++i){
   const unsigned offset=x*(width/400)*720+i;
   const unsigned expected=source[offset];
   assert(storage.pixels[x*720+i]==expected);
  }
 }
}
'''
with tempfile.TemporaryDirectory(prefix='stream-capture-width-') as directory:
    p = Path(directory)
    (p / 'test.cpp').write_text(code)
    subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined',
                    str(p / 'test.cpp'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('PASS: production display capture preserves the complete 400/800-column view within the bounded pixel buffer.')
