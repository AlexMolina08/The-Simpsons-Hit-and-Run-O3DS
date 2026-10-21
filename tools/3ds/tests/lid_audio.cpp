#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <set>
#include <sys/stat.h>
using u32=uint32_t;using u64=uint64_t;
enum APT_HookType { APTHOOK_ONSLEEP,APTHOOK_ONWAKEUP,APTHOOK_ONEXIT,APTHOOK_ONRESTORE };
struct aptHookCookie {};
typedef void (*HookType)(APT_HookType,void*);
HookType hook=nullptr;
void aptHook(aptHookCookie*,HookType value,void*) { hook=value; }
void aptUnhook(aptHookCookie*) { hook=nullptr; }
u64 svcGetSystemTick() { return 987654321; }
enum { NDSP_WBUF_DONE,NDSP_INTERP_LINEAR,NDSP_FORMAT_MONO_PCM16 };
struct ndspWaveBuf { void* data_vaddr;u32 nsamples;int status; };
bool g_NdspAvailable=true;
int allocations=0,channels=0,plays=0,clears=0;
int AllocChannel() { ++channels;return 5; }
void FreeChannel(int id) { assert(id==5);--channels; }
void* linearMemAlign(unsigned bytes,unsigned alignment) { void* out=nullptr;assert(!posix_memalign(&out,alignment,bytes));++allocations;return out; }
void linearFree(void* data) { --allocations;free(data); }
void DSP_FlushDataCache(void*,unsigned) {}
void ndspChnWaveBufClear(int id) { assert(id==5);++clears; }
void ndspChnReset(int id) { assert(id==5); }
void ndspChnSetInterp(int id,int) { assert(id==5); }
void ndspChnSetRate(int id,float rate) { assert(id==5 && rate==24000); }
void ndspChnSetFormat(int id,int format) { assert(id==5 && format==NDSP_FORMAT_MONO_PCM16); }
void ndspChnSetPaused(int id,bool value) { assert(id==5 && !value); }
void ndspChnWaveBufAdd(int id,ndspWaveBuf* wave) { assert(id==5 && wave->nsamples==320);++plays;wave->status=4; }
void ndspChnSetMix(int id,float* mix) { assert(id==5 && mix[0]>=0 && mix[0]<=0.6f && mix[0]==mix[1]); }
#include "lidgreeting.hpp"

int main()
{
    using namespace ctrLidGreeting;
    State selection;
    selection.Seed(12345);
    int previous=-1;
    for(int cycle=0;cycle<1000;++cycle) {
        std::set<unsigned> seen;
        for(int i=0;i<State::ClipCount;++i) { unsigned next=selection.Next();assert(int(next)!=previous);assert(seen.insert(next).second);previous=int(next); }
        assert(seen.size()==State::ClipCount);
    }
    State another;another.Seed(54321);
    selection.Seed(12345);bool differs=false;
    for(int i=0;i<State::ClipCount;++i) differs|=selection.Next()!=another.Next();
    assert(differs);
    mkdir("sound",0700);mkdir("sound/3ds-lid",0700);
    for(unsigned i=0;i<State::ClipCount;++i) {
        char path[80];snprintf(path,sizeof(path),"sound/3ds-lid/%02u.bin",i);
        FILE* file=fopen(path,"wb");assert(file);fwrite("LID1",1,4,file);
        u32 format[]={24000,320};fwrite(format,4,2,file);
        short samples[320]={0};fwrite(samples,2,320,file);fclose(file);
    }
    Initialize();assert(channels==1 && hook);
    Hook(APTHOOK_ONWAKEUP,nullptr);Service(1);assert(plays==0);
    Hook(APTHOOK_ONRESTORE,nullptr);Service(1);assert(plays==0);
    Hook(APTHOOK_ONSLEEP,nullptr);Hook(APTHOOK_ONWAKEUP,nullptr);Hook(APTHOOK_ONWAKEUP,nullptr);
    assert(plays==0 && allocations==0); // callback performs no file/DSP work
    Service(1);assert(plays==1 && allocations==1);
    Service(1);assert(plays==1 && allocations==1);
    Hook(APTHOOK_ONSLEEP,nullptr);Hook(APTHOOK_ONWAKEUP,nullptr);Service(0.5f);
    assert(plays==2 && allocations==1); // rapid close/open replaces, never overlaps
    wave.status=NDSP_WBUF_DONE;Service(1);assert(allocations==0);
    Hook(APTHOOK_ONSLEEP,nullptr);Hook(APTHOOK_ONWAKEUP,nullptr);Service(0);Service(1);assert(plays==2);
    Hook(APTHOOK_ONSLEEP,nullptr);Hook(APTHOOK_ONWAKEUP,nullptr);Hook(APTHOOK_ONSLEEP,nullptr);
    Service(1);assert(plays==2);Hook(APTHOOK_ONWAKEUP,nullptr);Service(1);assert(plays==3);
    Shutdown();assert(!hook && channels==0 && allocations==0);
    Initialize();g_NdspAvailable=false;Hook(APTHOOK_ONSLEEP,nullptr);Hook(APTHOOK_ONWAKEUP,nullptr);Service(1);
    assert(allocations==0 && plays==3);g_NdspAvailable=true;Shutdown();
    puts("PASS: 1000 shuffled cycles, no repeats, one cue per sleep/wake, HOME silence, mute, rapid reopen, bounded channel and buffer lifecycle.");
}
