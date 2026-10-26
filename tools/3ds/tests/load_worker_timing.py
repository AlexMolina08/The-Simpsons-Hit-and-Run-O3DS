#!/usr/bin/env python3
"""Check worker-only ownership slices without counting time yielded to main."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
source=(root/'libs/radcontent/src/radload/manager.cpp').read_text()
a=source.index('namespace {\nradLoadTiming::State workerTiming;')
b=source.index('extern "C" void radLoadDumpWorkerSlices',a)
production=source[a:b]
code=r'''
#define SRR2_OLD3DS_NATIVE_BENCH
#include <cassert>
#include <cstring>
#include <radload/loadtiming.hpp>
static uint64_t now=10;static unsigned reads=0;
uint64_t svcGetSystemTick(){++reads;return now;}
namespace NativeBench {
bool serviceProfile=true,collecting=true,main=false;
bool OnCaptureThread(){return main;}unsigned CurrentRouteMs(){return 123;}
}
PRODUCTION
int main(){
 {WorkerRegion file("art/chars/test.p3d");now+=5;PauseWorkerTiming();
  NativeBench::main=true;now=1000;{WorkerRegion mainCallback("ignored");}
  NativeBench::main=false;ResumeWorkerTiming();now+=7;
  {WorkerRegion callback("completion_callback");now+=1;PauseWorkerTiming();
   now=2000;ResumeWorkerTiming();now+=3;}
  now+=5;
 }
 assert(workerTiming.count==5 && workerTiming.samples==5);
 assert(workerTiming.longest[0].ticks==7);
 assert(!strcmp(workerTiming.longest[0].label,"art/chars/test.p3d"));
 uint64_t sum=0;for(unsigned i=0;i<workerTiming.count;++i)sum+=workerTiming.longest[i].ticks;
 assert(sum==21); // Excludes the 985 and 992 ticks spent yielded to main.
 char owned[]="original_filename";
 {WorkerRegion safe(owned);strcpy(owned,"changed");now+=100;}
 assert(!strcmp(workerTiming.longest[0].label,"original_filename"));
 {WorkerRegion file("chunked_file");
  now+=2;
  {radLoadTiming::StageRegion outer(0x10000);now+=200;
   {radLoadTiming::StageRegion inner(0x19000);now+=300;PauseWorkerTiming();
    now+=10000;ResumeWorkerTiming();now+=150;}
   now+=50;}
  now+=3;
 }
 assert(workerTiming.longest[0].ticks==300 && workerTiming.longest[0].stage==0x19000);
 assert(workerTiming.longest[1].ticks==200 && workerTiming.longest[1].stage==0x10000);
 assert(workerTiming.longest[2].ticks==150 && workerTiming.longest[2].stage==0x19000);
 assert(!strcmp(workerTiming.longest[0].label,"chunked_file"));
 auto before=reads;NativeBench::serviceProfile=false;
 {WorkerRegion disabled("disabled");radLoadTiming::StageRegion chunk(1);PauseWorkerTiming();ResumeWorkerTiming();}
 assert(reads==before);
 NativeBench::serviceProfile=true;NativeBench::main=true;
 {WorkerRegion foreign("main");radLoadTiming::StageRegion chunk(1);PauseWorkerTiming();ResumeWorkerTiming();}
 assert(reads==before);
 radLoadTiming::State bounded;
 for(unsigned i=1;i<=40;++i){bounded.Enter("longest",100*i,0);bounded.Leave(nullptr,100*i+i,0);}
 assert(bounded.count==16 && bounded.samples==40 && bounded.longest[0].ticks==40);
 for(unsigned i=1;i<bounded.count;++i)assert(bounded.longest[i-1].ticks>=bounded.longest[i].ticks);
 char label[200];memset(label,'x',sizeof(label));label[199]=0;
 bounded.Enter(label,10000,0);bounded.Leave(nullptr,20000,0);
 assert(strlen(bounded.longest[0].label)==95);
}
'''.replace('PRODUCTION',production)
with tempfile.TemporaryDirectory(prefix='srr-load-worker-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'libs/radcontent/src'),'-I'+str(root/'libs/radcontent/inc'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual worker observer excludes yielded time, restores nested file/callback labels, ignores main/disabled scopes and bounds its longest-slice records.')
