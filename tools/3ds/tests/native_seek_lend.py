#!/usr/bin/env python3
"""Exercise the production seek lease with real threads, failures and teardown."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
manager=(root/'libs/radcontent/src/radload/manager.cpp').read_text()
texture=(root/'libs/pure3d/pddi/pica/texture.cpp').read_text()
def function(source,signature):
    a=source.index(signature);b=source.index('{',a);end=b+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[a:end]
start=manager.index('namespace radNativeSeek {\nbool enabled=false;')
end=manager.index('\n#endif',start)
stats=manager[start:end]
start=manager.index('namespace radNativeSeek {\nvoid* Begin()')
end=manager.index('\n#endif',start)
api=manager[start:end]
methods='\n'.join(function(manager,s) for s in (
    'bool radLoadManager::BeginNativeSeek()',
    'void radLoadManager::EndNativeSeek()',
    'void radLoadManager::SwitchTasks()'))
start=texture.index('            positioned=input.file && entry->bytes<=picaNativeBundle::BundleBytes')
end=texture.index('\n        }',start)
seek=texture[start:end]
assert '__atomic_store_n(&manager->m_nativeSeekWorker,getThreadLocalStorage(),__ATOMIC_RELEASE)' in manager
assert 'radNativeSeek::Drain();' in (root/'code/main/nativebench.cpp').read_text()
fixture=r'''
#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <radnativeseek.hpp>
using u64=uint64_t;
constexpr double SYSCLOCK_ARM11=1000000000.0;
thread_local uintptr_t token=1;
void* getThreadLocalStorage(){return reinterpret_cast<void*>(token);}
std::atomic<unsigned> clockReads{0};
u64 svcGetSystemTick(){++clockReads;return std::chrono::steady_clock::now().time_since_epoch().count();}
namespace NativeBench {
std::atomic<bool> collecting{true};
bool OnCaptureThread(){return token==1;}
}
struct Mutex {
    std::mutex mutex;std::atomic<uintptr_t> owner{0};
    void Lock(){mutex.lock();owner=token;}
    void Unlock(){assert(owner==token);owner=0;mutex.unlock();}
    bool TryLock(){if(!mutex.try_lock())return false;owner=token;return true;}
};
Mutex original,other;
struct radLoadManager {
    void* m_nativeSeekWorker=nullptr;void* m_pCurrent=nullptr;bool m_bDone=false;
    Mutex* m_pMutex=&original;
    bool BeginNativeSeek();void EndNativeSeek();void SwitchTasks();
};
std::atomic<radLoadManager*> selected{nullptr};
radLoadManager* radLoadInstance(){return selected.load();}
void PauseWorkerTiming(){assert(original.owner==token);}
void ResumeWorkerTiming(){assert(original.owner==token);}
void radThreadSleep(unsigned){std::this_thread::yield();}
#define NB_SCOPE(s) ((void)0)
#define O3_POOL(a,b) ((void)0)
STATS
API
METHODS
namespace picaNativeBundle {constexpr unsigned BundleBytes=8192;}
struct Entry{unsigned offset=2500,bytes=128;};
std::mutex ioMutex;std::condition_variable ioCV;
bool blockIO=false,entered=false,allowIO=false;
int WaitSeek(FILE* file,long offset,int origin){
    std::unique_lock<std::mutex> lock(ioMutex);
    if(blockIO){if(radNativeSeek::owner){const unsigned before=radNativeSeek::begun;radNativeSeek::Lease nested;assert(radNativeSeek::begun==before);}entered=true;ioCV.notify_all();assert(ioCV.wait_for(lock,std::chrono::seconds(3),[]{return allowIO;}));}
    lock.unlock();return std::fseek(file,offset,origin);
}
#define fseek WaitSeek
bool Position(FILE* file,Entry* entry){struct {FILE* file;} input{file};bool positioned;
SEEK
return positioned;
}
#undef fseek
void WaitForIO(){std::unique_lock<std::mutex> lock(ioMutex);assert(ioCV.wait_for(lock,std::chrono::seconds(3),[]{return entered;}));}
void AllowIO(){std::lock_guard<std::mutex> lock(ioMutex);allowIO=true;ioCV.notify_all();}
int main(){
    radLoadManager manager,replacement;replacement.m_pMutex=&other;
    __atomic_store_n(&manager.m_nativeSeekWorker,reinterpret_cast<void*>(2),__ATOMIC_RELEASE);
    __atomic_store_n(&replacement.m_nativeSeekWorker,reinterpret_cast<void*>(3),__ATOMIC_RELEASE);
    int request=1;manager.m_pCurrent=&request;selected=&manager;
    FILE* file=tmpfile();assert(file);for(unsigned i=0;i<8192;++i)fputc(i&255,file);fflush(file);
    Entry entry;
    // Main and unrelated threads keep their original ownership and do no timing.
    radNativeSeek::enabled=true;original.Lock();auto clocks=clockReads.load();
    assert(Position(file,&entry)&&original.owner==1&&clockReads==clocks);
    original.Unlock();
    std::thread unrelated([&]{token=3;original.Lock();auto before=clockReads.load();
        assert(Position(file,&entry)&&original.owner==3&&clockReads==before);original.Unlock();});unrelated.join();
    auto blocked=[&](bool enabled,bool collecting,bool job,bool done){
        radNativeSeek::enabled=enabled;NativeBench::collecting=collecting;
        manager.m_pCurrent=job?&request:nullptr;manager.m_bDone=done;selected=&manager;
        {std::lock_guard<std::mutex> lock(ioMutex);blockIO=true;entered=allowIO=false;}
        const unsigned before=radNativeSeek::begun;clocks=clockReads;
        std::thread worker([&]{token=2;original.Lock();assert(Position(file,&entry));
            assert(original.owner==2);for(unsigned i=0;i<128;++i)assert(fgetc(file)==int((2500+i)&255));original.Unlock();});
        WaitForIO();const bool lends=enabled&&collecting&&job&&!done;
        unsigned frames=0;
        for(unsigned i=0;i<40;++i){if(original.TryLock()){++frames;original.Unlock();}}
        assert(lends?frames==40:frames==0);
        if(lends){
            // A nested lease must not unlock twice. End must use its captured
            // manager rather than a newly selected global manager.
            selected=&replacement;
        }
        AllowIO();worker.join();selected=&manager;
        assert(radNativeSeek::owner==nullptr&&original.owner==0);
        assert(radNativeSeek::begun==before+unsigned(lends));
        assert(radNativeSeek::completed==radNativeSeek::begun);
        if(!lends)assert(clockReads==clocks);
    };
    blocked(false,true,true,false);blocked(true,false,true,false);
    blocked(true,true,false,false);blocked(true,true,true,true);blocked(true,true,true,false);
    // Invalid metadata never lends. A real ESPIPE failure still reclaims first.
    manager.m_pCurrent=&request;manager.m_bDone=false;NativeBench::collecting=true;radNativeSeek::enabled=true;
    {std::lock_guard<std::mutex> lock(ioMutex);blockIO=false;}
    std::thread failures([&]{token=2;original.Lock();const unsigned before=radNativeSeek::begun;
        Entry invalid{9000,1};assert(!Position(file,&invalid));assert(radNativeSeek::begun==before);
        assert(!Position(nullptr,&entry));assert(radNativeSeek::begun==before);
        int descriptors[2];assert(pipe(descriptors)==0);FILE* stream=fdopen(descriptors[0],"r");assert(stream);
        assert(!Position(stream,&entry));assert(original.owner==2);fclose(stream);close(descriptors[1]);
        original.Unlock();});failures.join();
    // Production Drain restores the paused-worker assumption before teardown.
    {std::lock_guard<std::mutex> lock(ioMutex);blockIO=true;entered=allowIO=false;}
    std::thread worker([&]{token=2;original.Lock();assert(Position(file,&entry));const unsigned before=radNativeSeek::begun;assert(Position(file,&entry)&&radNativeSeek::begun==before);original.Unlock();});
    WaitForIO();original.Lock();
    std::thread completion([]{token=3;std::this_thread::sleep_for(std::chrono::milliseconds(10));AllowIO();});
    radNativeSeek::Drain();assert(original.owner==1&&radNativeSeek::owner==nullptr);original.Unlock();
    worker.join();completion.join();
    assert(radNativeSeek::begun==radNativeSeek::completed&&radNativeSeek::maxOutside>0);
    FILE* report=tmpfile();radNativeSeek::Dump(report);rewind(report);char text[1024]={};
    assert(fread(text,1,sizeof(text)-1,report)>0&&strstr(text,"active=0"));fclose(report);fclose(file);
}
'''.replace('STATS',stats).replace('API',api).replace('METHODS',methods).replace('SEEK\n',seek+'\n')
with tempfile.TemporaryDirectory(prefix='srr-native-seek-') as directory:
    p=Path(directory);(p/'test.cpp').write_text(fixture)
    for sanitizer in ('address,undefined','thread'):
        subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-pthread',
            '-fsanitize='+sanitizer,'-DSRR2_OLD3DS_NATIVE_BENCH',
            '-I'+str(root/'libs/radcontent/inc'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True,timeout=20)
print('PASS: production seek, worker-only lease, unchanged file bytes, default-off/noncapture/no-job/shutdown rejection, concurrent main frames, exact-owner reclaim, failed/invalid seeks and teardown drain under ASan/UBSan and TSan.')
