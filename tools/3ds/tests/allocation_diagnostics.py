#!/usr/bin/env python3
"""Exercise production allocation wrappers against a serialized allocator oracle."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='srr-memory-') as directory:
 p=Path(directory)
 (p/'3ds.h').write_text('''#include <mutex>
struct RecursiveLock{};void RecursiveLock_Init(RecursiveLock*);void RecursiveLock_Lock(RecursiveLock*);void RecursiveLock_Unlock(RecursiveLock*);
inline unsigned linearSpaceFree(){return 1000;}
const unsigned MEMREGION_APPLICATION=1;inline unsigned osGetMemRegionSize(unsigned){return 64*1024*1024;}inline unsigned osGetMemRegionFree(unsigned){return 0;}inline unsigned osGetApplicationMemType(){return 0;}
''')
 (p/'reent.h').write_text('struct _reent {};\n')
 (p/'malloc.h').write_text('struct mallinfo {int uordblks,arena;};struct mallinfo mallinfo();\n')
 source=r'''
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include <thread>
#include <string>
extern "C" {unsigned long __malloc_max_sbrked_mem=8*1024*1024;
unsigned long __malloc_max_total_mem=8*1024*1024;}
#include "@SOURCE@"
static std::recursive_mutex gate;static std::map<void*,size_t> sizes;static unsigned used,oraclePeak;static bool refuse;
void RecursiveLock_Init(RecursiveLock*){}
void RecursiveLock_Lock(RecursiveLock*){gate.lock();}
void RecursiveLock_Unlock(RecursiveLock*){gate.unlock();}
extern "C" void __malloc_lock(_reent*){gate.lock();}
extern "C" void __real___malloc_unlock(_reent*){gate.unlock();}
struct AllocatorGuard{AllocatorGuard(){__malloc_lock(nullptr);}~AllocatorGuard(){__wrap___malloc_unlock(nullptr);}};
struct mallinfo mallinfo(){AllocatorGuard guard;return {int(used),8*1024*1024};}
void track(void* p,size_t n){if(p){sizes[p]=n;used+=n;if(used>oraclePeak)oraclePeak=used;}}
extern "C" void* __real__malloc_r(_reent*,size_t n){AllocatorGuard guard;if(refuse)return nullptr;void* p=malloc(n);track(p,n);return p;}
extern "C" void __real__free_r(_reent*,void* p){AllocatorGuard guard;if(p){used-=sizes.at(p);sizes.erase(p);free(p);}}
extern "C" void* __real__calloc_r(_reent*,size_t n,size_t s){AllocatorGuard guard;void* p=calloc(n,s);track(p,n*s);return p;}
extern "C" void* __real__memalign_r(_reent*,size_t alignment,size_t n){AllocatorGuard guard;void* p=nullptr;assert(posix_memalign(&p,alignment,n)==0);track(p,n);return p;}
extern "C" void* __real__realloc_r(_reent*,void* p,size_t n){AllocatorGuard guard;if(refuse)return nullptr;size_t old=p?sizes.at(p):0;if(p)sizes.erase(p);void* q=realloc(p,n);used-=old;track(q,n);return q;}
int main(){
 _reent r;void* transient=__wrap__malloc_r(&r,6*1024*1024);__wrap__free_r(&r,transient);assert(used==0);
 Old3dsMemorySetPhase(O3MemoryLoading);void* loading=__wrap__malloc_r(&r,3*1024*1024);__wrap__free_r(&r,loading);assert(phaseNormal[O3MemoryLoading]==3*1024*1024);Old3dsMemorySetPhase(O3MemoryRoute);
 void* c=__wrap__calloc_r(&r,5,19);for(unsigned i=0;i<95;i++)assert(((unsigned char*)c)[i]==0);
 c=__wrap__realloc_r(&r,c,217);refuse=true;assert(!__wrap__realloc_r(&r,c,300));refuse=false;__wrap__free_r(&r,c);
 void* aligned=__wrap__memalign_r(&r,256,1024);assert((uintptr_t(aligned)%256)==0);__wrap__free_r(&r,aligned);
 auto worker=[&](){_reent local;for(unsigned i=0;i<500;i++){void* q=__wrap__malloc_r(&local,32+i);__wrap__free_r(&local,q);}};
 std::thread a(worker),b(worker);a.join();b.join();assert(used==0&&sizes.empty());
 assert(peak==oraclePeak&&peak==6*1024*1024);assert(normalFailures==1);
 Old3dsMemoryLinear(1000,false);Old3dsMemoryLinear(400,false);Old3dsMemoryLinear(700,true);
 FILE* f=tmpfile();Old3dsMemoryDump(f);rewind(f);char line[2048];assert(fgets(line,sizeof line,f));
 assert(strstr(line,"normal_used_peak=6291456")&&strstr(line,"linear_free_min=400")&&strstr(line,"linear_failures=1"));
 assert(fgets(line,sizeof line,f));assert(strstr(line,"newlib_sbrk_peak=8388608")&&strstr(line,"sampled_used_within_commitment=1")&&strstr(line,"heap_walk=0"));fclose(f);
 Old3dsMemorySetStride(64);unsigned prior=observations;
 for(unsigned i=0;i<1024;++i){void* p=__wrap__malloc_r(&r,123);__wrap__free_r(&r,p);}
 assert(observations-prior==32);
 Old3dsMemorySetPhase(O3MemoryReload);assert(phaseArena[O3MemoryReload]==8*1024*1024);
 f=tmpfile();Old3dsMemoryDump(f);rewind(f);assert(fgets(line,sizeof line,f));fclose(f);
 assert(strstr(line,"normal_stride=64")&&strstr(line,"normal_used_is_sampled_lower_bound=1"));
 puts("PASS: transient peaks, concurrent allocation/free serialization, realloc failure ownership, alignment, calloc bytes and linear minima preserved.");
}
'''.replace('@SOURCE@',str(root/'code/main/old3dsmemory.cpp'))
 (p/'test.cpp').write_text(source)
 subprocess.run(['c++','-std=c++17','-pthread','-fsanitize=address,undefined','-DSRR2_OLD3DS_MEMORY_DIAG','-I'+str(p),'-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
