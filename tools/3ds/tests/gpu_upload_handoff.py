#!/usr/bin/env python3
"""Exercise the production FrameBegin handoff with loader uploads at each boundary."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/pddi/pica/display.cpp').read_text()
a=source.index('        bool frameOk;',source.index('void picaDisplay::BeginFrame'))
b=source.index('\n\n        if(!frameOk)',a)
block=source[a:b]
code=r'''
#include <cassert>
#include <pddi/pica/gpuuploadguard.hpp>
namespace picaUpload { unsigned gpuHandoff=0; }
static bool succeed,sdkInFrame=false;
static unsigned loans,reclaims,begins;
static void LoaderWouldUpload(){assert(picaUpload::Blocked());}
static void Hook(int lend){
 LoaderWouldUpload();
 if(lend)++loans;
 else {assert(sdkInFrame==succeed);++reclaims;}
}
static void (*g_picaGpuWaitHook)(int)=Hook;
static bool C3D_FrameBegin(unsigned flags){
 assert(flags==0);LoaderWouldUpload();++begins;
 sdkInFrame=succeed;
 // Citro3D may be active while the backend's public in-frame flag is false.
 LoaderWouldUpload();return succeed;
}
static bool Exercise(bool result){
 succeed=result;sdkInFrame=false;
 BLOCK
 assert(!picaUpload::Blocked());return frameOk;
}
int main(){
 for(unsigned i=0;i<100;++i){assert(Exercise(true));assert(!Exercise(false));}
 assert(loans==200&&reclaims==200&&begins==200&&!picaUpload::Blocked());
 g_picaGpuWaitHook=nullptr;assert(Exercise(true));assert(!Exercise(false));
 {picaUpload::Handoff outer;assert(picaUpload::Blocked());
  {picaUpload::Handoff inner;assert(picaUpload::Blocked());}
  assert(picaUpload::Blocked());
 }
 assert(!picaUpload::Blocked());
}
'''.replace('BLOCK',block)
with tempfile.TemporaryDirectory(prefix='srr-gpu-upload-handoff-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DSRR2_OLD3DS_VRAM_TEXTURES','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: production FrameBegin blocks loader uploads before lending, during SDK begin, and through reclaim; success/failure/null hooks and nested restoration are safe.')
