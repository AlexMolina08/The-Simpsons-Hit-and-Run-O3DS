#!/usr/bin/env python3
"""Exercise production benchmark readback with a fake SDK and an asymmetric image."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/main/nativebench.cpp').read_text()
function=source[source.index('static void DumpVisual(FILE* f)'):source.index('void Dump(){if(!enabled)return;')]
fixture=r'''
#include <cstdio>
#include <cassert>
#include <cstring>
using u16=unsigned short;
#define R_SUCCEEDED(value) ((value)>=0)
enum GSPGPU_FramebufferFormat { GSP_BGR8_OES=1 };
enum {GFX_TOP,GFX_LEFT};
static bool visualCapture=false;
static unsigned syncs=0,invalidations=0;
static int cacheResult=0;
static unsigned char pixels[240*400*3];
static u16 rawWidth=240,rawHeight=400;
static GSPGPU_FramebufferFormat selected=GSP_BGR8_OES;
void C3D_FrameSync(){++syncs;}
GSPGPU_FramebufferFormat gfxGetScreenFormat(int){return selected;}
unsigned char* gfxGetFramebuffer(int,int,u16* w,u16* h){*w=rawWidth;*h=rawHeight;return pixels;}
int GSPGPU_InvalidateDataCache(void* p,unsigned n){assert(p==pixels && n==sizeof(pixels));++invalidations;return cacheResult;}
'''
main=r'''
int main(){
 FILE* f=tmpfile();assert(f);DumpVisual(f);assert(ftell(f)==0 && syncs==0 && invalidations==0);fclose(f);
 visualCapture=true;
 // Asymmetric source: physical top-left red, top-right green, bottom-left blue.
 for(unsigned x=0;x<400;++x)for(unsigned y=0;y<240;++y){
  unsigned char* p=pixels+(x*240+239-y)*3;p[0]=y==239?255:0;p[1]=x==399?255:0;p[2]=(x==0 && y==0)?255:0;
 }
 f=tmpfile();assert(f);DumpVisual(f);assert(syncs==1 && invalidations==1);rewind(f);
 char buffer[4096];size_t n;while((n=fread(buffer,1,sizeof(buffer),f)))fwrite(buffer,1,n,stdout);fclose(f);
 cacheResult=-1;f=tmpfile();DumpVisual(f);assert(syncs==2 && invalidations==2);rewind(f);
 char failed[512]={};assert(fread(failed,1,sizeof(failed)-1,f)>0);assert(std::strstr(failed,"valid=0") && !std::strstr(failed,"NATIVE_VISUAL_BYTES"));fclose(f);
 rawWidth=239;f=tmpfile();DumpVisual(f);assert(syncs==3 && invalidations==2);fclose(f);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(fixture+function+main)
 subprocess.run(['c++','-std=c++17','-O2',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 raw=subprocess.check_output([str(p/'test')],text=True)
 chunks=re.findall(r'^NATIVE_VISUAL_BYTES offset=(\d+) data=([0-9a-f]+)$',raw,re.M)
 output=bytearray()
 for offset,data in chunks:
  assert int(offset)==len(output);output.extend(bytes.fromhex(data))
 assert len(output)==288000 and 'NATIVE_VISUAL_END bytes=288000' in raw
 def pixel(x,y):return tuple(output[(x*240+239-y)*3:(x*240+239-y)*3+3][::-1])
 assert pixel(0,0)==(255,0,0) and pixel(399,0)==(0,255,0) and pixel(0,239)==(0,0,255)
print('Production readback: default-off, synchronization, bounds, complete hex payload and native orientation passed.')
