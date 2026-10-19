/* Immutable PICA command lists. Protocol verified on physical Old 3DS by
 * gpu-list-probe-125: target channel 0, return channel 1, exact return spans.
 * No nested calls, command reordering, worker transport or in-flight mutation.
 */
#include "nativegpulist.h"
#include <stdlib.h>
#include <string.h>
enum { Slots=256, Ways=4, ReturnCapacity=1024 };
typedef struct {
    u32 hash,lastFrame;
    unsigned keyWords,words,payloadWords;
    u32 key[PICA_GPU_LIST_KEY_WORDS];
} Entry;
static Entry* entries;
static u32* lists;
static unsigned frame;
static bool attempted;
static struct {u32* size;u32* continuation;} returns[ReturnCapacity];
static unsigned returnCount;
static picaGpuListStats stats;
bool g_picaGpuListsEnabled;
bool g_picaGpuListsValidate;
extern void __real_GPUCMD_Split(u32**,u32*);
void __wrap_GPUCMD_Split(u32** address,u32* words)
{
    __real_GPUCMD_Split(address,words);
    u32* end=*address+*words;
    for(unsigned i=0;i<returnCount;++i) {
        u32* stop=i+1<returnCount?returns[i+1].continuation:end;
        if(returns[i].continuation<*address || stop>end ||
           stop<=returns[i].continuation || ((stop-returns[i].continuation)&3))abort();
        *returns[i].size=(u32)(stop-returns[i].continuation)*4>>3;
    }
    returnCount=0;
}
void picaGpuListsBeginFrame(void)
{
    // Called only after C3D_FrameBegin has waited for the previous GPU queue.
    if(returnCount)abort();
    if(!attempted) {
        attempted=true;
        entries=calloc(Slots,sizeof(*entries));
        lists=linearAlloc(Slots*PICA_GPU_LIST_WORDS*sizeof(u32));
        if(!entries || !lists){free(entries);linearFree(lists);entries=NULL;lists=NULL;}
    }
    if(++frame==0){if(entries)memset(entries,0,Slots*sizeof(*entries));frame=1;}
}
bool picaGpuListRoom(void)
{
    return entries && lists && returnCount<ReturnCapacity && gpuCmdBuf &&
        gpuCmdBufOffset<=gpuCmdBufSize && gpuCmdBufSize-gpuCmdBufOffset>=2048;
}
u32* picaGpuListFind(const u32* key,unsigned words,bool* hit)
{
    *hit=false;
    if(!picaGpuListRoom() || words>PICA_GPU_LIST_KEY_WORDS){++stats.fallbacks;return NULL;}
    u32 hash=2166136261u;
    for(unsigned i=0;i<words;++i)hash=(hash^key[i])*16777619u;
    unsigned first=(hash%(Slots/Ways))*Ways,victim=Slots;
    for(unsigned i=first;i<first+Ways;++i) {
        Entry* e=entries+i;
        if(e->words && e->hash==hash && e->keyWords==words && !memcmp(e->key,key,words*4)) {
            e->lastFrame=frame;*hit=true;++stats.hits;return lists+i*PICA_GPU_LIST_WORDS;
        }
        if(e->lastFrame!=frame && (victim==Slots || !e->words || e->lastFrame<entries[victim].lastFrame))victim=i;
    }
    if(victim==Slots){++stats.fallbacks;return NULL;}
    Entry* e=entries+victim;e->hash=hash;e->lastFrame=frame;e->keyWords=words;e->words=0;
    memcpy(e->key,key,words*4);++stats.misses;
    return lists+victim*PICA_GPU_LIST_WORDS;
}
void picaGpuListCommit(u32* list,unsigned words)
{
    if(words+4>PICA_GPU_LIST_WORDS || (words&1))abort();
    entries[(list-lists)/PICA_GPU_LIST_WORDS].payloadWords=words;
    // An inert channel-size write pads the immutable return command to 16 bytes.
    if((words+2)&3){list[words++]=0;list[words++]=GPUCMD_HEADER(0,15,GPUREG_CMDBUF_SIZE0);}
    list[words++]=1;list[words++]=GPUCMD_HEADER(0,15,GPUREG_CMDBUF_JUMP1);
    entries[(list-lists)/PICA_GPU_LIST_WORDS].words=words;
    GSPGPU_FlushDataCache(list,words*4);
}
bool picaGpuListCompare(const u32* list,const u32* reference,unsigned words)
{
    ++stats.comparisons;
    bool same=entries[(list-lists)/PICA_GPU_LIST_WORDS].payloadWords==words && !memcmp(list,reference,words*4);
    if(!same)++stats.mismatches;
    return same;
}
void picaGpuListCall(u32* list)
{
    if(returnCount>=ReturnCapacity || !gpuCmdBuf || gpuCmdBufSize-gpuCmdBufOffset<16)abort();
    if(gpuCmdBufOffset&3)GPUCMD_AddWrite(GPUREG_CMDBUF_SIZE0,0);
    const unsigned continuation=gpuCmdBufOffset+12;
    const unsigned words=entries[(list-lists)/PICA_GPU_LIST_WORDS].words;
    u32 call[]={0,GPUCMD_HEADER(0,15,GPUREG_CMDBUF_SIZE0),
        words*4>>3,GPUCMD_HEADER(0,15,GPUREG_CMDBUF_SIZE0),
        0,GPUCMD_HEADER(0,15,GPUREG_CMDBUF_SIZE1),
        osConvertVirtToPhys(list)>>3,GPUCMD_HEADER(0,15,GPUREG_CMDBUF_ADDR0),
        osConvertVirtToPhys(gpuCmdBuf+continuation)>>3,GPUCMD_HEADER(0,15,GPUREG_CMDBUF_ADDR1),
        1,GPUCMD_HEADER(0,15,GPUREG_CMDBUF_JUMP0)};
    returns[returnCount].size=gpuCmdBuf+gpuCmdBufOffset+4;
    returns[returnCount++].continuation=gpuCmdBuf+continuation;
    memcpy(gpuCmdBuf+gpuCmdBufOffset,call,sizeof(call));gpuCmdBufOffset+=12;
    ++stats.calls;stats.retainedWords+=words;
}
void picaGpuListsGetStats(picaGpuListStats* output){*output=stats;}
