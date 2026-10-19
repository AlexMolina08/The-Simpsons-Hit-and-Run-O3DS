#ifdef SRR2_OLD3DS_RENDER_WORKER
#include "renderworker.h"
#include "nativecontext.h"
#include "nativecontextsnapshot.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define SLOT_COUNT 16u
#define WORD_LIMIT 2048u
typedef struct {
    unsigned kind,reserved,words,rawWords;
    picaNativeContextSnapshot context;
    u32 output[2*WORD_LIMIT];
} Slot;
typedef struct {
    Slot slots[SLOT_COUNT];
    unsigned published,compiled,committed,owner,quit,reserved;
    u32* buffer;
    Thread thread;
    LightEvent wake,finished;
    bool attempted,open;
} State;
unsigned g_picaRenderWorkerMode;
static State* state;
static bool frameActive,disabled;
static unsigned bypass;
static picaRenderWorkerStats statistics;
static bool recording(void){return state && state->thread && frameActive && !disabled && !bypass && g_picaRenderWorkerMode==3;}

static void require(bool condition,const char* reason)
{
    if(condition)return;
    fprintf(stderr,"RENDER_WORKER_INVARIANT reason=%s\n",reason);fflush(stderr);
    exit(1);
}
static void seal(State* s);
static void drain(State* s,bool worker)
{
    unsigned idle=0;
    if(!__atomic_compare_exchange_n(&s->owner,&idle,1u,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return;
    for(;;) {
        const unsigned index=__atomic_load_n(&s->compiled,__ATOMIC_RELAXED);
        if(index==__atomic_load_n(&s->published,__ATOMIC_SEQ_CST))break;
        Slot* slot=&s->slots[index%SLOT_COUNT];
        if(slot->kind==1) {
            picaNativeCommandBlock output={slot->output,0};
            picaNativeCompileContext(&output,&slot->context,picaNativeSharedUniformCache());
            require(output.count<=WORD_LIMIT,"compiled_context_capacity");
            memmove(slot->output+output.count,slot->output+WORD_LIMIT,slot->rawWords*sizeof(u32));
            slot->words=output.count+slot->rawWords;
            if(worker){++statistics.workerContexts;statistics.workerCore=svcGetProcessorID();}else ++statistics.mainContexts;
        }
        __atomic_store_n(&s->compiled,index+1u,__ATOMIC_SEQ_CST);
    }
    __atomic_store_n(&s->owner,0u,__ATOMIC_RELEASE);
    LightEvent_Signal(&s->finished);
}
static void worker(void* argument)
{
    State* s=(State*)argument;
    for(;;) {
        LightEvent_Wait(&s->wake);
        if(__atomic_load_n(&s->quit,__ATOMIC_ACQUIRE))return;
        drain(s,true);
    }
}
static State* allocateState(void)
{
    if(!state) {
        state=(State*)calloc(1,sizeof(State));
        if(state){LightEvent_Init(&state->wake,RESET_ONESHOT);LightEvent_Init(&state->finished,RESET_ONESHOT);}
    }
    return state;
}
static bool initialize(State* s)
{
    if(s->attempted)return s->thread!=NULL;
    s->attempted=true;
    u32 initial=0;APT_GetAppCpuTimeLimit(&initial);statistics.initialAllowance=initial;
    statistics.initResult=(unsigned)APT_SetAppCpuTimeLimit(80);
    if(R_FAILED((Result)statistics.initResult))return false;
    u32 actual=0;APT_GetAppCpuTimeLimit(&actual);statistics.actualAllowance=actual;
    s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);if(priority<0x3f)++priority;
    s->thread=threadCreate(worker,s,32768,priority,1,false);
    if(!s->thread){statistics.threadFailed=1;APT_SetAppCpuTimeLimit(initial?initial:30);}
    return s->thread!=NULL;
}
static void commit(State* s)
{
    const unsigned end=__atomic_load_n(&s->compiled,__ATOMIC_SEQ_CST);
    if(s->committed==end)return;
    const u64 start=svcGetSystemTick();
    // Split/end wrappers must drain before the SDK can replace this buffer.
    require(gpuCmdBuf==s->buffer,"unfenced_buffer_change");
    while(s->committed!=end) {
        Slot* slot=&s->slots[s->committed%SLOT_COUNT];
        require(gpuCmdBufOffset<=gpuCmdBufSize && slot->words<=gpuCmdBufSize-gpuCmdBufOffset,"commit_capacity");
        memcpy(gpuCmdBuf+gpuCmdBufOffset,slot->output,slot->words*sizeof(u32));
        gpuCmdBufOffset+=slot->words;s->reserved-=slot->reserved;++s->committed;
    }
    statistics.commitTicks+=svcGetSystemTick()-start;
}
void picaRenderWorkerFence(void)
{
    State* s=state;if(!s)return;seal(s);
    if(s->committed==__atomic_load_n(&s->published,__ATOMIC_SEQ_CST))return;
    ++statistics.fences;
    const u64 start=svcGetSystemTick();
    while(__atomic_load_n(&s->compiled,__ATOMIC_SEQ_CST)!=__atomic_load_n(&s->published,__ATOMIC_SEQ_CST)) {
        // Assist only while holding the same exclusive compiler lease. A late
        // core-1 scheduling slice cannot force an otherwise idle main to wait.
        drain(s,false);
        if(__atomic_load_n(&s->compiled,__ATOMIC_SEQ_CST)!=__atomic_load_n(&s->published,__ATOMIC_SEQ_CST))
            LightEvent_Wait(&s->finished);
    }
    statistics.waitTicks+=svcGetSystemTick()-start;commit(s);
}
static bool reserve(State* s,unsigned words)
{
    unsigned used=__atomic_load_n(&s->published,__ATOMIC_RELAXED)-s->committed;
    if(used==SLOT_COUNT || !gpuCmdBuf || gpuCmdBufOffset>gpuCmdBufSize ||
       words>gpuCmdBufSize-gpuCmdBufOffset || s->reserved>gpuCmdBufSize-gpuCmdBufOffset-words) {
        ++statistics.capacityFences;picaRenderWorkerFence();used=0;
    }
    if(!gpuCmdBuf || gpuCmdBufOffset>gpuCmdBufSize || words>gpuCmdBufSize-gpuCmdBufOffset)return false;
    if(!used)s->buffer=gpuCmdBuf;
    require(gpuCmdBuf==s->buffer,"unfenced_buffer_change");
    if(used+1>statistics.peakSlots)statistics.peakSlots=used+1;
    return true;
}
static void publish(State* s,Slot* slot,unsigned reserved)
{
    slot->reserved=reserved;s->reserved+=reserved;
    const unsigned previous=__atomic_fetch_add(&s->published,1u,__ATOMIC_SEQ_CST);
    ++statistics.records;if(slot->kind==0)++statistics.rawRecords;
    // A nonempty compiler queue already has a reader or an earlier wake. Signal
    // only an empty-to-nonempty transition, including a reader winding down.
    // Sequential publication/completion checks prevent both sides observing
    // stale empty/nonempty counters and losing the wake on weakly ordered ARM.
    if(__atomic_load_n(&s->compiled,__ATOMIC_SEQ_CST)==previous){++statistics.notifications;LightEvent_Signal(&s->wake);}
}
static void seal(State* s)
{
    if(!s->open)return;
    Slot* slot=&s->slots[__atomic_load_n(&s->published,__ATOMIC_RELAXED)%SLOT_COUNT];
    // Remove producer ownership before reserve can recursively drain older jobs.
    s->open=false;
    const unsigned bound=(slot->kind==1?WORD_LIMIT:0)+slot->rawWords;
    if(reserve(s,bound)) {
        if(slot->kind==0)slot->words=slot->rawWords;
        publish(s,slot,bound);return;
    }
    // Conservative pending bounds can exceed remaining space while the actual
    // compiled block still fits. Drain first, then commit the exact block.
    picaRenderWorkerFence();
    if(slot->kind==1) {
        picaNativeCommandBlock output={slot->output,0};
        picaNativeCompileContext(&output,&slot->context,picaNativeSharedUniformCache());
        require(output.count<=WORD_LIMIT,"serial_context_capacity");
        memmove(slot->output+output.count,slot->output+WORD_LIMIT,slot->rawWords*sizeof(u32));
        slot->words=output.count+slot->rawWords;++statistics.mainContexts;
    } else slot->words=slot->rawWords;
    require(gpuCmdBuf && gpuCmdBufOffset<=gpuCmdBufSize && slot->words<=gpuCmdBufSize-gpuCmdBufOffset,"serial_commit_capacity");
    memcpy(gpuCmdBuf+gpuCmdBufOffset,slot->output,slot->words*sizeof(u32));gpuCmdBufOffset+=slot->words;
}
static Slot* rawSlot(State* s,unsigned bound)
{
    if(s->open) {
        Slot* slot=&s->slots[__atomic_load_n(&s->published,__ATOMIC_RELAXED)%SLOT_COUNT];
        if(bound<=WORD_LIMIT-slot->rawWords)return slot;
        seal(s);
    }
    if(!reserve(s,bound))return NULL;
    Slot* slot=&s->slots[__atomic_load_n(&s->published,__ATOMIC_RELAXED)%SLOT_COUNT];
    slot->kind=0;slot->rawWords=0;s->open=true;return slot;
}
void picaRenderWorkerBeginFrame(void)
{picaRenderWorkerFence();frameActive=true;}
void picaRenderWorkerEndFrame(void)
{picaRenderWorkerFence();frameActive=false;}
void picaRenderWorkerShutdown(void)
{
    if(!state)return;picaRenderWorkerEndFrame();
    if(state->thread) {
        __atomic_store_n(&state->quit,1u,__ATOMIC_RELEASE);LightEvent_Signal(&state->wake);
        threadJoin(state->thread,U64_MAX);threadFree(state->thread);
        APT_SetAppCpuTimeLimit(statistics.initialAllowance?statistics.initialAllowance:30);
    }
    free(state);state=NULL;
}
static bool sameCache(const picaNativeUniformCache* a,const picaNativeUniformCache* b)
{
    return !memcmp(a->values,b->values,sizeof(a->values)) && !memcmp(a->valid,b->valid,sizeof(a->valid)) &&
        !memcmp(a->fast,b->fast,sizeof(a->fast)) && a->avoided==b->avoided && a->sent==b->sent;
}
bool picaRenderWorkerContext(void)
{
    if(!frameActive || disabled || bypass || !g_picaRenderWorkerMode)return false;
    if(!picaNativeCanCaptureContext() || !gpuCmdBuf || gpuCmdBufOffset>gpuCmdBufSize ||
       gpuCmdBufSize-gpuCmdBufOffset<WORD_LIMIT || (gpuCmdBufOffset&1)) {
        picaRenderWorkerFence();return false;
    }
    State* s=allocateState();if(!s)return false;
    if(g_picaRenderWorkerMode==3) {
        if(!initialize(s)){picaRenderWorkerFence();return false;}
        seal(s);
        if(!reserve(s,WORD_LIMIT)){picaRenderWorkerFence();return false;}
        Slot* slot=&s->slots[__atomic_load_n(&s->published,__ATOMIC_RELAXED)%SLOT_COUNT];
        slot->kind=1;slot->rawWords=0;picaNativeCaptureContext(&slot->context);++statistics.contexts;s->open=true;return true;
    }
    Slot* slot=&s->slots[0];
    if(g_picaRenderWorkerMode==2) {
        picaNativeUniformProducerState before,after,actual;
        C3D_Context beforeContext=*C3Di_GetContext();
        picaNativeSaveUniformProducer(&before);
        picaNativeUniformCache cache=*picaNativeSharedUniformCache();
        picaNativeCaptureContext(&slot->context);
        C3D_Context afterContext=*C3Di_GetContext();picaNativeSaveUniformProducer(&after);
        picaNativeCommandBlock output={slot->output,0};picaNativeCompileContext(&output,&slot->context,&cache);
        *C3Di_GetContext()=beforeContext;picaNativeRestoreUniformProducer(&before);
        const unsigned first=gpuCmdBufOffset;++bypass;picaNativeUpdateContext();--bypass;
        picaNativeSaveUniformProducer(&actual);++statistics.comparisons;
        const C3D_Context* actualContext=C3Di_GetContext();
        if(output.count!=gpuCmdBufOffset-first || memcmp(slot->output,gpuCmdBuf+first,output.count*sizeof(u32)) ||
           afterContext.flags!=actualContext->flags || afterContext.texConfig!=actualContext->texConfig ||
           !picaNativeSameUniformProducer(&after,&actual) || !sameCache(&cache,picaNativeSharedUniformCache())) {
            ++statistics.mismatches;disabled=true;
        }
        return true;
    }
    picaNativeCaptureContext(&slot->context);
    picaNativeCommandBlock output={gpuCmdBuf+gpuCmdBufOffset,0};
    picaNativeCompileContext(&output,&slot->context,picaNativeSharedUniformCache());
    require(output.count<=WORD_LIMIT,"synchronous_context_capacity");gpuCmdBufOffset+=output.count;++statistics.contexts;++statistics.mainContexts;return true;
}
extern void __real_GPUCMD_Add(u32,const u32*,u32);
void __wrap_GPUCMD_Add(u32 header,const u32* values,u32 count)
{
    if(recording() && values && count && count<=1024 && !(gpuCmdBufOffset&1)) {
        const unsigned bound=count+3*((count+255)/256);
        Slot* slot=rawSlot(state,bound);
        if(slot) {
            picaNativeCommandBlock output={slot->output+(slot->kind==1?WORD_LIMIT:0),slot->rawWords};
            picaNativeCommandAdd(&output,header,values,count);
            require(output.count<=WORD_LIMIT,"raw_command_capacity");
            slot->rawWords=output.count;++statistics.raw;return;
        }
    }
    picaRenderWorkerFence();__real_GPUCMD_Add(header,values,count);
}
extern void __real_GPUCMD_AddRawCommands(const u32*,u32);
void __wrap_GPUCMD_AddRawCommands(const u32* words,u32 count)
{
    if(recording() && words && count && count<=WORD_LIMIT && !(count&1) && !(gpuCmdBufOffset&1)) {
        Slot* slot=rawSlot(state,count);
        if(slot) {
            u32* output=slot->output+(slot->kind==1?WORD_LIMIT:0);
            memcpy(output+slot->rawWords,words,count*sizeof(u32));slot->rawWords+=count;++statistics.raw;return;
        }
    }
    picaRenderWorkerFence();__real_GPUCMD_AddRawCommands(words,count);
}
extern void __real_GPUCMD_Split(u32**,u32*);
void __wrap_GPUCMD_Split(u32** address,u32* size)
{picaRenderWorkerFence();++bypass;__real_GPUCMD_Split(address,size);--bypass;}
extern void __real_C3D_FrameSplit(u8);
void __wrap_C3D_FrameSplit(u8 flags)
{picaRenderWorkerFence();++bypass;__real_C3D_FrameSplit(flags);--bypass;}
extern void __real_C3D_FrameEnd(u8);
void __wrap_C3D_FrameEnd(u8 flags)
{picaRenderWorkerFence();++bypass;__real_C3D_FrameEnd(flags);--bypass;}
void picaRenderWorkerGetStats(picaRenderWorkerStats* output)
{picaRenderWorkerFence();*output=statistics;}
void picaRenderWorkerDump(FILE* file)
{
    picaRenderWorkerStats s;picaRenderWorkerGetStats(&s);
    fprintf(file,"NATIVE_RENDER_WORKER contexts=%u raw=%u records=%u raw_records=%u notifications=%u worker_contexts=%u main_contexts=%u fences=%u capacity_fences=%u comparisons=%u mismatches=%u peak_slots=%u initial_allowance=%u actual_allowance=%u worker_core=%u init_result=%08x thread_failed=%u wait_ms=%.6f commit_ms=%.6f\n",s.contexts,s.raw,s.records,s.rawRecords,s.notifications,s.workerContexts,s.mainContexts,s.fences,s.capacityFences,s.comparisons,s.mismatches,s.peakSlots,s.initialAllowance,s.actualAllowance,s.workerCore,s.initResult,s.threadFailed,s.waitTicks*1000.0/SYSCLOCK_ARM11,s.commitTicks*1000.0/SYSCLOCK_ARM11);
}
#endif
