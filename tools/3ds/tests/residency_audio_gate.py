#!/usr/bin/env python3
"""Verify real admission scoping lets audio reads pass without unguarded opens."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root/'libs/radcore/src/radfile/sdl/sdldrive.cpp').read_text()

def structure(name):
    start = source.index('struct '+name+'\n')
    return source[start:source.index('\n};', start)+3]

start = source.index('                const int admitted = [&] {')
end = source.index('\n                char prepared[', start)
admission = source[start:end]
program = r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace std::chrono_literals;
using LightLock=int;
std::mutex mutex;LightLock s_FileGate=1;
thread_local bool owns=false;
void LightLock_Lock(LightLock*){assert(!owns);mutex.lock();owns=true;}
void LightLock_Unlock(LightLock*){assert(owns);owns=false;mutex.unlock();}
''' + structure('Srr2FileGate')+'\n'+structure('Srr2FileGateRelease')+r'''
std::promise<void>* startAudio;
std::future<void>* finishedAudio;
int admittedResult=0;
bool throwAdmission=false,readDuringValidation=false;
unsigned opened=0;
namespace WorldResidencyAssets {unsigned Level(const char*){return 1;}}
int old3dsWorldResidencyAdmitStep(unsigned level){
 assert(level==1 && !owns);
 startAudio->set_value();
 readDuringValidation=finishedAudio->wait_for(1s)==std::future_status::ready;
 if(throwAdmission)throw std::runtime_error("validation failed");
 return admittedResult;
}
enum {MoreToDo=2};
int Open(){
 Srr2FileGate gate;
 for(const char* canonical:{"l1_TERRA.p3d"}){
''' + admission+r'''
  assert(owns);++opened;
 }
 assert(owns);
 return 1;
}
int main(){
 for(int result:{-1,0,1})for(bool failure:{false,true}){
  admittedResult=result;throwAdmission=failure;readDuringValidation=false;
  std::promise<void> begin,done;auto beginFuture=begin.get_future();auto doneFuture=done.get_future();
  startAudio=&begin;finishedAudio=&doneFuture;
  std::thread audio([&]{beginFuture.wait();{Srr2FileGate gate;assert(owns);}done.set_value();});
  const unsigned previous=opened;
  try{assert(Open()==(result==0?MoreToDo:1));assert(!failure);}catch(const std::runtime_error&){assert(failure);}
  audio.join();assert(readDuringValidation && !owns);
  assert(opened==previous+unsigned(result>0 && !failure));
  // All ordinary I/O and tracked-handle bookkeeping keep the original guard.
  {Srr2FileGate gate;assert(owns);}assert(!owns);
 }
 puts("PASS: audio reads finish during admission; accepted, rejected and exceptional exits restore the file lock.");
}
'''
with tempfile.TemporaryDirectory(prefix='residency-audio-gate-') as directory:
    path = Path(directory)
    (path/'check.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++17', '-O1', '-g', '-pthread', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', str(path/'check.cpp'), '-o', str(path/'check')], check=True)
    subprocess.run([str(path/'check')], check=True)
