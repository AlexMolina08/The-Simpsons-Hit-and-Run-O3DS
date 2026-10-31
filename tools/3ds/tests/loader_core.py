#!/usr/bin/env python3
"""Exercise private core selection, native thread lifecycle and safe fallback."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/radcontent/src/radload/manager.cpp').read_text()
start = source.index('// CoreExperimentBegin:')
end = source.index('// CoreExperimentEnd.', start)
policy = source[start:end]
termination = source[source.index('void radLoadManager::Terminate()'):]
assert termination.index('m_pThread->WaitForTermination()') < termination.index('coreExperiment.Restore()')
code = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
using Result=int;using u32=unsigned;using u8=unsigned char;
#define R_SUCCEEDED(x) ((x)>=0)
static const int CUR_THREAD_HANDLE=1;
static u32 budget=0;static u8 affinity=3;
static bool allowBudget=true;static int currentCore=0;
static unsigned budgetCalls=0;
Result APT_GetAppCpuTimeLimit(u32* out){*out=budget;return 0;}
Result APT_SetAppCpuTimeLimit(u32 value){++budgetCalls;if(!allowBudget)return -7;budget=value;return 0;}
Result svcGetThreadAffinityMask(u8* out,int thread,int count){assert(thread==1&&count==2);out[0]=affinity;out[1]=0;return 0;}
int svcGetProcessorID(){return currentCore;}
POLICY
int main(){
 LoaderCoreExperiment normal;normal.Apply();
 assert(budgetCalls==0&&normal.CreationCore()==-1&&normal.budgetAfter==0);
 for(int core:{0,1}){
  budget=0;budgetCalls=0;
  LoaderCoreExperiment candidate;candidate.Request(core);
  assert(candidate.CreationCore()==core&&budget==30&&budgetCalls==1);
  affinity=1u<<core;currentCore=core;candidate.Apply();candidate.Sample();
  assert(candidate.budgetBefore==0&&candidate.budgetAfter==30&&candidate.before[0]==affinity);
  assert(candidate.after[0]==affinity&&candidate.after[1]==0);
  assert(candidate.observed[core]==1&&candidate.observed[1-core]==0);
  candidate.Restore();assert(budget==0&&budgetCalls==2);
 }
 allowBudget=false;budget=0;
 LoaderCoreExperiment denied;denied.Request(1);denied.Apply();
 assert(denied.budgetSet==-7&&denied.CreationCore()==-1);
 unsigned calls=budgetCalls;denied.Restore();assert(budgetCalls==calls);
 currentCore=9;denied.Sample();for(unsigned value:denied.observed)assert(value==0);
}
'''.replace('POLICY', policy)

thread_source = (root / 'libs/radcore/src/radthread/thread.cpp').read_text()
a = thread_source.index('    const size_t nativeStackSize =')
b = thread_source.index('    // NativeCoreCreateEnd.', a)
create = thread_source[a:b]
a = thread_source.index('void radThread::InternalNativeCoreEntry(')
a = thread_source.index('{', a)
b = thread_source.index('\n}\n', a)
entry = thread_source[a + 1:b]
a = thread_source.index('unsigned int radThread::WaitForTermination( void )')
a = thread_source.index('{', a)
b = thread_source.index('\n}\n', a)
join = thread_source[a + 1:b]
thread_test = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <cstddef>
#define RAD_3DS
#define SRR2_OLD3DS_NATIVE_BENCH
#define R_FAILED(x) ((x)<0)
using Result=int;using s32=int;
static constexpr unsigned long long U64_MAX=~0ULL;
static constexpr int CUR_THREAD_HANDLE=1;
struct Native {void(*entry)(void*);void* data;bool joined=false;};using Thread=Native*;
struct SDL_Thread{};static SDL_Thread sdlThread;
static bool allowNative=true;
static unsigned creates=0,sdlCreates=0,joins=0,frees=0,tlsCleanups=0,sdlWaits=0;
static int requestedCore=-1;static size_t stackBytes=0;
int svcGetThreadPriority(s32* p,int handle){assert(handle==1);*p=0x30;return 0;}
Thread threadCreate(void(*entry)(void*),void* data,size_t stack,int priority,int core,bool detached){
 ++creates;requestedCore=core;stackBytes=stack;assert(priority==0x30&&!detached);
 return allowNative?new Native{entry,data}:nullptr;
}
struct ThreadExit{};
void threadExit(int result){assert(result==0);throw ThreadExit{};}
void SDL_TLSCleanup(){++tlsCleanups;}
Result threadJoin(Thread thread,unsigned long long wait){
 assert(wait==U64_MAX&&!thread->joined);++joins;
 try{thread->entry(thread->data);assert(false);}catch(ThreadExit&){}
 thread->joined=true;return 0;
}
void threadFree(Thread thread){assert(thread->joined);++frees;delete thread;}
SDL_Thread* SDL_CreateThreadWithStackSize(int(*)(void*),const char*,size_t stack,void*){
 ++sdlCreates;stackBytes=stack;return &sdlThread;
}
void SDL_WaitThread(SDL_Thread* thread,int* result){assert(thread==&sdlThread);++sdlWaits;*result=99;}
struct radThread {
 void* m_NativeCoreThread=nullptr;SDL_Thread* m_ThreadHandle=nullptr;unsigned m_ReturnCode=0;
 static int InternalThreadEntry(void* data){static_cast<radThread*>(data)->m_ReturnCode=42;return 0;}
 static void InternalNativeCoreEntry(void* param){ENTRY}
 void Start(int core,unsigned stackSize){CREATE}
 unsigned WaitForTermination(){JOIN}
};
int main(){
 radThread normal;normal.Start(-1,65536);assert(creates==0&&sdlCreates==1&&stackBytes==131072);
 assert(normal.WaitForTermination()==99&&sdlWaits==1);
 for(int core:{0,1}){
  const unsigned priorSdl=sdlCreates,priorJoin=joins,priorFree=frees,priorTls=tlsCleanups;
  radThread worker;worker.Start(core,262144);
  assert(requestedCore==core&&stackBytes==262144&&worker.m_NativeCoreThread&&sdlCreates==priorSdl);
  assert(worker.WaitForTermination()==42&&!worker.m_NativeCoreThread);
  assert(joins==priorJoin+1&&frees==priorFree+1&&tlsCleanups==priorTls+1);
 }
 allowNative=false;const unsigned prior=sdlCreates;
 radThread fallback;fallback.Start(1,65536);
 assert(!fallback.m_NativeCoreThread&&sdlCreates==prior+1&&fallback.WaitForTermination()==99);
}
'''.replace('ENTRY', entry).replace('CREATE', create).replace('JOIN', join)
with tempfile.TemporaryDirectory(prefix='loader-core-') as directory:
    p = Path(directory)
    for label, contents in (('policy', code), ('native', thread_test)):
        cpp, executable = p / (label + '.cpp'), p / label
        cpp.write_text(contents)
        subprocess.run(['clang++', '-std=c++17', '-fsanitize=address,undefined',
                        str(cpp), '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
print('PASS: CPU-budget denial, packed affinity readback, actual native core creation, stack size, join/TLS/free lifecycle, SDL fallback and restore after join.')
