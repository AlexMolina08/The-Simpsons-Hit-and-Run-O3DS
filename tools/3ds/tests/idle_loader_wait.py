#!/usr/bin/env python3
"""Verify the actual pacing helper lends only idle time and guards GPU access."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/main/game.cpp').read_text()
start = source.index('static void Old3dsWaitForFrame(')
function = source[start:source.index('extern "C" unsigned g_picaPresentedFrames', start)]
code = r'''
#include <cassert>
#include <cstdint>
#include <pddi/pica/gpuuploadguard.hpp>
using s64=int64_t;
namespace picaUpload {unsigned gpuHandoff=0;}
namespace WorldStreaming {bool idleLoader=true;}
namespace NativeBench {bool IdleLoaderWait(){return WorldStreaming::idleLoader;}}
static bool held=true,pending=true,expectedHeld=false;
static unsigned lends=0,reclaims=0,sleeps=0;static s64 duration=0;
static void hook(int lend){
 assert(picaUpload::Blocked());
 if(lend){assert(held);++lends;if(pending)held=false;}
 else {++reclaims;held=true;}
}
static void (*g_picaGpuWaitHook)(int)=hook;
static void svcSleepThread(s64 ns){
 assert(held==expectedHeld);++sleeps;duration=ns;
}
''' + function + r'''
int main(){
 Old3dsWaitForFrame(1234567);
 assert(duration==1234567&&sleeps==1&&lends==1&&reclaims==1&&held&&!picaUpload::Blocked());
 pending=false;expectedHeld=true;Old3dsWaitForFrame(7654321);
 assert(duration==7654321&&sleeps==2&&held&&!picaUpload::Blocked());
 g_picaGpuWaitHook=nullptr;Old3dsWaitForFrame(19);assert(duration==19&&held&&!picaUpload::Blocked());
 g_picaGpuWaitHook=hook;picaUpload::gpuHandoff=1;Old3dsWaitForFrame(27);
 assert(duration==27&&held&&picaUpload::Blocked());picaUpload::gpuHandoff=0;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
 const unsigned before=lends;WorldStreaming::idleLoader=false;Old3dsWaitForFrame(31);
 assert(duration==31&&lends==before&&held&&!picaUpload::Blocked());
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='idle-loader-wait-') as directory:
    p = Path(directory)
    (p / 'test.cpp').write_text(code)
    for diagnostic in (False, True):
        args = ['c++', '-std=c++17', '-fsanitize=address,undefined',
                '-I' + str(root / 'libs/pure3d')]
        if diagnostic:
            args.append('-DSRR2_OLD3DS_NATIVE_BENCH')
        subprocess.run(args + [str(p / 'test.cpp'), '-o', str(p / 'test')], check=True)
        subprocess.run([str(p / 'test')], check=True)
print('PASS: production and diagnostic pacing preserve wait duration, lend/reclaim order, pending/idle/null-hook handling and nested GPU upload guards.')
