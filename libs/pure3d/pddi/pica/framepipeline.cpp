#include <pddi/pica/framepipeline.hpp>
#include <new>
extern "C" void picaNativeFramePrefix(bool);
extern "C" void picaNativeFrameRetirePrefix(void);
extern "C" void picaNativeFrameAbort(void);

namespace {
unsigned ReadFrameFpControls()
{
    unsigned value;
    __asm__ volatile("vmrs %0, fpscr":"=r"(value));
    return value&0x07f79f00u;
}
}
picaFramePipeline* picaFramePipeline::Create(picaPreparedExecutor* executor,picaPreparedInputPool* pool)
{
    if(!executor || !pool)return nullptr;
    auto* self=new(std::nothrow)picaFramePipeline;
    if(!self)return nullptr;
    self->pages[0].executor=executor;self->pages[0].pool=pool;
    LightEvent_Init(&self->wake,RESET_ONESHOT);LightEvent_Init(&self->finished,RESET_ONESHOT);
    u32 initial=0;APT_GetAppCpuTimeLimit(&initial);self->initialAllowance=initial;
    if(R_FAILED(APT_SetAppCpuTimeLimit(80))){delete self;return nullptr;}
    self->allowanceSet=true;
    u32 actual=0;APT_GetAppCpuTimeLimit(&actual);self->actualAllowance=actual;
    self->mainFp=ReadFrameFpControls();
    s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);
    if(priority<0x3f)++priority;
    self->thread=threadCreate(Worker,self,32768,priority,1,false);
    if(!self->thread){delete self;return nullptr;}
    while(!__atomic_load_n(&self->ready,__ATOMIC_ACQUIRE))LightEvent_Wait(&self->finished);
    if(self->mainFp!=self->workerFp){delete self;return nullptr;}
    return self;
}
picaFramePipeline::~picaFramePipeline()
{
    auto* executor=pages[0].executor;auto* pool=pages[0].pool;
    if(inFlight)Join(executor,pool);
    if(recording)Flush(executor,pool);
    if(thread){
        __atomic_store_n(&quit,1u,__ATOMIC_RELEASE);LightEvent_Signal(&wake);
        threadJoin(thread,U64_MAX);threadFree(thread);
    }
    if(allowanceSet)APT_SetAppCpuTimeLimit(initialAllowance?initialAllowance:30);
    delete originalPrimary;delete[] expectedCommands;
    picaNativeFreeCheckpoint(beforeCheckpoint);picaNativeFreeCheckpoint(expectedCheckpoint);picaNativeFreeCheckpoint(actualCheckpoint);
    // Primary page belongs to the context, as in the existing prepared backend.
    for(unsigned i=1;i<MaximumPages;++i){delete pages[i].executor;delete pages[i].pool;}
}
bool picaFramePipeline::Allocate(unsigned index)
{
    if(index>=MaximumPages)return false;
    Page& target=pages[index];
    if(target.executor && target.pool)return true;
    target.executor=new(std::nothrow)picaPreparedExecutor;
    target.pool=new(std::nothrow)picaPreparedInputPool;
    if(target.executor && target.pool)return true;
    delete target.executor;delete target.pool;target.executor=nullptr;target.pool=nullptr;
    return false;
}
void picaFramePipeline::Begin(picaPreparedExecutor*& executor,picaPreparedInputPool*& pool)
{
    picaPreparedExecutor::Require(!recording && !inFlight,"frame_begin_while_owned");
    page=0;executor=pages[0].executor;pool=pages[0].pool;
    picaPreparedExecutor::Require(executor->valid && !executor->operations,"frame_primary_not_imported");
    pool->Reset();failure=nullptr;recording=true;
}
void picaFramePipeline::Seal()
{
    Page& current=pages[page];
    current.operations=current.executor->operations;current.geometry=current.executor->geometry;
    current.palettes=current.executor->paletteCount;
}
bool picaFramePipeline::Advance(picaPreparedExecutor*& executor,picaPreparedInputPool*& pool)
{
    picaPreparedExecutor::Require(recording && !inFlight,"frame_advance_without_capture");
    if(!executor->operations){pool->Reset();return true;}
    if(!Allocate(page+1))return false;
    Seal();++page;executor=pages[page].executor;pool=pages[page].pool;
    executor->operations=executor->geometry=executor->paletteCount=executor->driverUsed=0;
    executor->valid=true;executor->draining=false;pool->Reset();
    return true;
}
void picaFramePipeline::Execute()
{
    const u64 start=svcGetSystemTick();
    picaPreparedExecutor* previous=pages[0].executor;
    const unsigned beforeDraws=previous->submitted;
    bool prefixRan=false;
    for(unsigned i=0;i<=page;++i){
        Page& current=pages[i];auto& executor=*current.executor;
        if(i)executor.AdoptConsumer(*previous);
        executor.operations=current.operations;executor.geometry=current.geometry;executor.paletteCount=current.palettes;
        // All capacity and palette failures return to the caller. Never exit
        // from core 1: an earlier worker exit stalled the physical netloader.
        if(!executor.TryFlush()){failure=executor.failureReason;break;}
        previous=&executor;
        // Split once around the middle of the owned frame. Prefix storage is
        // immutable; later commands append after it. Start GPU work while the
        // consumer finishes the remaining commands and main runs simulation.
        if(splitPrefix && page>0 && i+1==(page+1)/2){
            const bool run=present && !compare;picaNativeFramePrefix(run);prefixRan=run;if(run)++metrics.prefixFrames;
        }
    }
    result=previous;
    if(!failure){
        previous->End();
        if(compare){
            picaNativeSaveCheckpoint(actualCheckpoint);++metrics.comparisons;
            const u32* end=gpuCmdBuf+gpuCmdBufOffset;
            const bool same=end>=comparisonBase && (unsigned)(end-comparisonBase)==expectedWords &&
                !memcmp(expectedCommands,comparisonBase,expectedWords*sizeof(u32)) &&
                picaNativeCompareCheckpoint(expectedCheckpoint,actualCheckpoint) &&
                !memcmp(&expectedUniforms,&previous->uniforms,sizeof(expectedUniforms)) && !memcmp(&expectedState,&g_picaState,sizeof(expectedState));
            if(!same){
                ++metrics.mismatches;failure="frame_comparison_mismatch";
                unsigned actualWords=end>=comparisonBase?(unsigned)(end-comparisonBase):0,first=0;
                while(first<expectedWords && first<actualWords && expectedCommands[first]==comparisonBase[first])++first;
                printf("FRAME_COMPARE_FAILURE expected_words=%u actual_words=%u first_word=%u expected=%08lx actual=%08lx sdk=%u uniforms=%u state=%u\n",expectedWords,actualWords,first,(unsigned long)(first<expectedWords?expectedCommands[first]:0),(unsigned long)(first<actualWords?comparisonBase[first]:0),picaNativeCompareCheckpoint(expectedCheckpoint,actualCheckpoint),!memcmp(&expectedUniforms,&previous->uniforms,sizeof(expectedUniforms)),!memcmp(&expectedState,&g_picaState,sizeof(expectedState)));fflush(stdout);
            }
            else metrics.comparedDraws+=previous->submitted-beforeDraws;
        }
        if(!failure && present){
            // Retire the prefix before FrameEnd enqueues final commands and
            // installs swap/timing flags. Waiting precedes Stop/Clear: never
            // request an asynchronous stop against a running queue.
            if(prefixRan){const u64 waitStart=svcGetSystemTick();picaNativeFrameRetirePrefix();metrics.prefixWaitTicks+=svcGetSystemTick()-waitStart;}
            C3D_FrameEnd(GX_CMDLIST_FLUSH);++metrics.presented;
            metrics.processingMs=C3D_GetProcessingTime();metrics.drawingMs=C3D_GetDrawingTime();
        }
        metrics.draws+=previous->submitted-beforeDraws;
    }
    if(failure){if(prefixRan)picaNativeFrameAbort();++metrics.failures;}
    ++metrics.frames;metrics.pages+=page+1;
    if(page+1>metrics.maximumPages)metrics.maximumPages=page+1;
    metrics.executionTicks+=svcGetSystemTick()-start;
}
void picaFramePipeline::RestoreProducer(picaPreparedExecutor*& executor,picaPreparedInputPool*& pool)
{
#if defined(SRR2_OLD3DS_RETAINED_MATERIAL) || defined(SRR2_OLD3DS_DEFERRED_SKIN)
    for(unsigned i=0;i<=page;++i)pages[i].executor->ReleaseOwnedInputs();
#endif
    for(unsigned i=0;i<=page;++i){pages[i].executor->operations=pages[i].executor->geometry=pages[i].executor->paletteCount=pages[i].executor->driverUsed=0;pages[i].executor->valid=false;pages[i].executor->draining=false;}
    executor=pages[0].executor;pool=pages[0].pool;completedMetrics=metrics;
    if(failure && thread){
        __atomic_store_n(&quit,1u,__ATOMIC_RELEASE);LightEvent_Signal(&wake);
        threadJoin(thread,U64_MAX);threadFree(thread);thread=nullptr;
    }
    picaPreparedExecutor::Require(!failure,failure?failure:"frame_execution_failed");
}
picaPreparedExecutor* picaFramePipeline::Flush(picaPreparedExecutor*& executor,picaPreparedInputPool*& pool,const void* site)
{
    picaPreparedExecutor::Require(recording && !inFlight,"frame_flush_without_capture");
    Seal();unsigned pending=0;for(unsigned i=0;i<=page;++i)pending+=pages[i].geometry;
    const u64 start=site && pending?svcGetSystemTick():0;
    recording=false;present=compare=splitPrefix=false;Execute();++metrics.synchronous;
    if(site && pending){
        unsigned index=0;while(index<drainCount && drains[index].site!=site)++index;
        if(index==drainCount){
            if(drainCount<31){drains[index].site=site;++drainCount;}
            else{index=31;drainCount=32;drains[index].site=nullptr;}
        }
        auto& entry=drains[index];++entry.calls;entry.draws+=pending;entry.ticks+=svcGetSystemTick()-start;
    }
    RestoreProducer(executor,pool);return result;
}
void picaFramePipeline::Submit(bool submitFrame,bool compareFrame,bool gpuPrefix)
{
    picaPreparedExecutor::Require(recording && !inFlight,"frame_submit_without_capture");
    // Keep GPU submission under the SDK's normal FrameEnd protocol. libctru's
    // Stop is asynchronous while a command runs; Run returns early in that
    // state, so stopping an early prefix can strand FrameEnd's queued tail.
    // Optional prefix execution retires the completed queue before FrameEnd
    // adds its tail. No new commands enter that queue while the prefix runs.
    // Full-frame CPU ownership still overlaps the next simulation update.
    Seal();recording=false;splitPrefix=gpuPrefix && submitFrame && !compareFrame;
    if(compareFrame){
        u32* buffer;u32 size,offset;GPUCMD_GetBuffer(&buffer,&size,&offset);startOffset=offset;
        picaPreparedExecutor::Require(buffer && startOffset<=size,"frame_comparison_buffer");comparisonBase=buffer+startOffset;
        if(!originalPrimary){
            originalPrimary=new(std::nothrow)picaPreparedExecutor;
            beforeCheckpoint=picaNativeAllocateCheckpoint();expectedCheckpoint=picaNativeAllocateCheckpoint();actualCheckpoint=picaNativeAllocateCheckpoint();
        }
        if(size>expectedCapacity){delete[] expectedCommands;expectedCommands=new(std::nothrow)u32[size];expectedCapacity=size;}
        picaPreparedExecutor::Require(originalPrimary && beforeCheckpoint && expectedCheckpoint && actualCheckpoint && expectedCommands,"frame_comparison_allocation");
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
        for(unsigned i=0;i<=page;++i)pages[i].executor->ResetDeferredSkinCaches();
#endif
        *originalPrimary=*pages[0].executor;const picaStateCache beforeState=g_picaState;const Metrics beforeMetrics=metrics;
        picaNativeSaveCheckpoint(beforeCheckpoint);
        present=compare=false;Execute();
        picaPreparedExecutor::Require(!failure,failure?failure:"frame_reference_failed");
        picaNativeSaveCheckpoint(expectedCheckpoint);expectedUniforms=result->uniforms;expectedState=g_picaState;
        expectedWords=(unsigned)(gpuCmdBuf+gpuCmdBufOffset-comparisonBase);memcpy(expectedCommands,comparisonBase,expectedWords*sizeof(u32));
        picaNativeRestoreCheckpoint(beforeCheckpoint);g_picaState=beforeState;*pages[0].executor=*originalPrimary;metrics=beforeMetrics;
        gpuCmdBuf=buffer;gpuCmdBufSize=size;gpuCmdBufOffset=startOffset;
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
        // Serial reference must not warm the worker's bone calculation cache.
        for(unsigned i=0;i<=page;++i)pages[i].executor->ResetDeferredSkinCaches();
#endif
    }
    inFlight=true;present=submitFrame;compare=compareFrame;
    __atomic_add_fetch(&request,1u,__ATOMIC_RELEASE);LightEvent_Signal(&wake);
}
picaPreparedExecutor* picaFramePipeline::Join(picaPreparedExecutor*& executor,picaPreparedInputPool*& pool)
{
    picaPreparedExecutor::Require(inFlight,"frame_join_without_job");
    const u64 start=svcGetSystemTick();const unsigned target=__atomic_load_n(&request,__ATOMIC_ACQUIRE);
    while(__atomic_load_n(&complete,__ATOMIC_ACQUIRE)!=target)LightEvent_Wait(&finished);
    inFlight=false;metrics.joinTicks+=svcGetSystemTick()-start;
    RestoreProducer(executor,pool);return result;
}
void picaFramePipeline::Worker(void* argument)
{
    auto* self=static_cast<picaFramePipeline*>(argument);
    self->workerFp=ReadFrameFpControls();self->core=svcGetProcessorID();
    __atomic_store_n(&self->ready,1u,__ATOMIC_RELEASE);LightEvent_Signal(&self->finished);
    for(;;){
        LightEvent_Wait(&self->wake);
        if(__atomic_load_n(&self->quit,__ATOMIC_ACQUIRE))return;
        const unsigned job=__atomic_load_n(&self->request,__ATOMIC_ACQUIRE);
        if(job==__atomic_load_n(&self->complete,__ATOMIC_RELAXED))continue;
        self->Execute();
        __atomic_store_n(&self->complete,job,__ATOMIC_RELEASE);LightEvent_Signal(&self->finished);
    }
}

void picaFramePipeline::NoteJoin(const void* site,u64 cpuTicks,u64 gpuTicks)
{
    if(!site)return;
    unsigned index=0;while(index<joinSiteCount && joinSites[index].site!=site)++index;
    if(index==joinSiteCount){
        if(joinSiteCount<31){joinSites[index].site=site;++joinSiteCount;}
        else{index=31;joinSiteCount=32;joinSites[index].site=nullptr;}
    }
    auto& entry=joinSites[index];++entry.calls;entry.cpuTicks+=cpuTicks;entry.gpuTicks+=gpuTicks;
}
