#!/usr/bin/env python3
"""Exercise production scope nesting and disabled observer behavior."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'code/main/nativebench.cpp').read_text()
a=s.index('void Scope::BeginScope(');b=s.index('bool ResidentShaders()',a)
code=r'''
#include <cassert>
#include <main/nativebench.h>
unsigned reads=0;u64 tick=1;
u64 svcGetSystemTick(){++reads;return tick;}
namespace NativeBench {
bool detail=false,collecting=true,validation=false,serviceProfile=false;
bool captureThread=true;
bool OnCaptureThread(){return captureThread;}
unsigned counters[CountC]={};
struct Record {unsigned stage[Count]={},exclusive[Count]={};} current;
static Scope* top=nullptr;
static void Checkpoint(const char*,int){}
'''+s[a:b]+r'''
}
int main(){
 using namespace NativeBench;
 {Scope parent(Service);tick+=3;{Scope child(FileService);tick+=11;}tick+=5;}
 assert(reads==2 && current.stage[Service]==19 && current.exclusive[Service]==19);
 assert(!current.stage[FileService] && !top);
 current={};tick=1;reads=0;serviceProfile=true;
 {Scope parent(Service);tick+=3;
  {Scope child(FileService);tick+=11;}
  {Scope child(DebugService);tick+=7;}
  {Scope child(SoundService);tick+=101;}
  tick+=5;
 }
 assert(reads==8 && current.stage[Service]==127 && current.exclusive[Service]==8);
 assert(current.stage[FileService]==11 && current.stage[DebugService]==7 && current.stage[SoundService]==101);
 assert(current.exclusive[FileService]==11 && current.exclusive[SoundService]==101 && !top);
 captureThread=false;const auto wrongThreadReads=reads;
 {Scope child(FileLock);tick+=5;}
 assert(reads==wrongThreadReads);captureThread=true;
 collecting=false;const auto before=reads;
 {Scope parent(Service);{Scope child(FileService);}}
 assert(reads==before);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-service-profile-') as td:
 p=Path(td);(p/'3ds.h').write_text('#pragma once\n#include <cstdint>\nusing u64=uint64_t;u64 svcGetSystemTick();\n');(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-DSRR2_OLD3DS_NATIVE_BENCH','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(p),'-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: production service scopes, disjoint attribution, disabled observers and post-frame stack restoration.')
