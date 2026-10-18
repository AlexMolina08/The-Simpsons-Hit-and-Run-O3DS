#include <pddi/pica/preparedworker.hpp>
#include <new>

namespace {
unsigned ReadFpControls()
{
    unsigned value;
    // Read only. Changing floating-point controls caused a prior hardware fault.
    __asm__ volatile("vmrs %0, fpscr":"=r"(value));
    return value&0x07f79f00u;
}
}
picaPreparedWorker* picaPreparedWorker::Create(picaPreparedExecutor* executor,picaPreparedInputPool* pool)
{
    picaPreparedWorker* result=new(std::nothrow)picaPreparedWorker;
    if(!result)return nullptr;
    result->slots[0].executor=executor;result->slots[0].pool=pool;
    result->slots[1].executor=new(std::nothrow)picaPreparedExecutor;
    result->slots[1].pool=new(std::nothrow)picaPreparedInputPool;
    for(auto& slot:result->slots)slot.output=new(std::nothrow)u32[OutputWords];
    if(!result->slots[1].executor || !result->slots[1].pool || !result->slots[0].output || !result->slots[1].output){delete result;return nullptr;}
    LightEvent_Init(&result->wake,RESET_ONESHOT);LightEvent_Init(&result->finished,RESET_ONESHOT);
    u32 initial=0;APT_GetAppCpuTimeLimit(&initial);result->initialAllowance=initial;
    Result status=APT_SetAppCpuTimeLimit(80);
    if(R_FAILED(status)){delete result;return nullptr;}
    result->allowanceSet=true;
    u32 actual=0;APT_GetAppCpuTimeLimit(&actual);result->actualAllowance=actual;
    result->mainFp=ReadFpControls();
    s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);if(priority<0x3f)++priority;
    result->thread=threadCreate(Worker,result,32768,priority,1,false);
    if(!result->thread){delete result;return nullptr;}
    while(!__atomic_load_n(&result->ready,__ATOMIC_ACQUIRE))LightEvent_Wait(&result->finished);
    printf("PREPARED_WORKER_INIT core=%u main_fp=%08x worker_fp=%08x allowance=%u\n",result->workerCore,result->mainFp,result->workerFp,result->actualAllowance);fflush(stdout);
    if(result->workerFp!=result->mainFp){delete result;return nullptr;}
    return result;
}
picaPreparedWorker::~picaPreparedWorker()
{
    if(thread){
        picaPreparedExecutor* executor=slots[producer].executor;picaPreparedInputPool* pool=slots[producer].pool;
        if(active)Finish(executor,pool);
        __atomic_store_n(&quit,1u,__ATOMIC_RELEASE);LightEvent_Signal(&wake);
        threadJoin(thread,U64_MAX);threadFree(thread);
    }
    if(allowanceSet)APT_SetAppCpuTimeLimit(initialAllowance?initialAllowance:30);
    delete reference;delete[] referenceOutput;
    picaNativeFreeCheckpoint(beforeCheckpoint);picaNativeFreeCheckpoint(expectedCheckpoint);picaNativeFreeCheckpoint(actualCheckpoint);
    delete slots[1].executor;delete slots[1].pool;
    delete[] slots[0].output;delete[] slots[1].output;
}
void picaPreparedWorker::Worker(void* argument)
{
    auto* self=static_cast<picaPreparedWorker*>(argument);
    self->workerFp=ReadFpControls();self->workerCore=svcGetProcessorID();
    __atomic_store_n(&self->ready,1u,__ATOMIC_RELEASE);LightEvent_Signal(&self->finished);
    for(;;){
        LightEvent_Wait(&self->wake);
        if(__atomic_load_n(&self->quit,__ATOMIC_ACQUIRE))return;
        self->Drain(true);
    }
}
void picaPreparedWorker::Begin(bool compareOutput)
{
    picaPreparedExecutor::Require(!active && published==committed,"worker_begin_while_owned");
    GPUCMD_GetBuffer(&liveBuffer,&liveSize,&liveOffset);
    picaPreparedExecutor::Require(liveBuffer && liveOffset<=liveSize && !(liveOffset&1),"worker_live_buffer");
    compare=compareOutput;
    if(compare && !reference){
        reference=new(std::nothrow)picaPreparedExecutor;referenceOutput=new(std::nothrow)u32[OutputWords];
        beforeCheckpoint=picaNativeAllocateCheckpoint();expectedCheckpoint=picaNativeAllocateCheckpoint();actualCheckpoint=picaNativeAllocateCheckpoint();
        picaPreparedExecutor::Require(reference && referenceOutput && beforeCheckpoint && expectedCheckpoint && actualCheckpoint,"worker_comparison_allocation");
    }
    producer=published%2;last=slots[producer].executor;reserved=0;
    picaPreparedExecutor::Require(last==slots[0].executor,"worker_primary_arena");
    // Sequence numbers continue across fences. The next lease starts at primary.
    producer=0;
    slots[1].executor->valid=true;slots[1].pool->Reset();
    active=true;
}
void picaPreparedWorker::Drain(bool worker)
{
    unsigned idle=0;
    if(!__atomic_compare_exchange_n(&owner,&idle,1u,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return;
    for(;;){
        if(__atomic_load_n(&failure,__ATOMIC_ACQUIRE))break;
        unsigned index=__atomic_load_n(&completed,__ATOMIC_RELAXED);
        if(index==__atomic_load_n(&published,__ATOMIC_SEQ_CST))break;
        Slot& slot=slots[index%2];picaPreparedExecutor& executor=*slot.executor;
        executor.AdoptConsumer(*last);
        unsigned expectedWords=0;
        if(compare){
            picaNativeSaveCheckpoint(beforeCheckpoint);*reference=executor;reference->AdoptConsumer(executor);
            gpuCmdBuf=referenceOutput;gpuCmdBufSize=OutputWords;gpuCmdBufOffset=0;
            reference->Flush();expectedWords=gpuCmdBufOffset;picaNativeSaveCheckpoint(expectedCheckpoint);
            picaNativeRestoreCheckpoint(beforeCheckpoint);
        }
        gpuCmdBuf=slot.output;gpuCmdBufSize=OutputWords;gpuCmdBufOffset=0;
        unsigned before=executor.submitted;executor.Flush();
        slot.words=gpuCmdBufOffset;
        if(compare){
            picaNativeSaveCheckpoint(actualCheckpoint);++comparisons;
            const bool same=expectedWords==slot.words && !memcmp(referenceOutput,slot.output,slot.words*sizeof(u32)) &&
                picaNativeCompareCheckpoint(expectedCheckpoint,actualCheckpoint) &&
                !memcmp(&reference->uniforms,&executor.uniforms,sizeof(executor.uniforms)) && !memcmp(&reference->state,&executor.state,sizeof(executor.state));
            if(!same){
                ++mismatches;unsigned first=0;while(first<expectedWords && first<slot.words && referenceOutput[first]==slot.output[first])++first;
                printf("PREPARED_COMPARE_FAILURE expected_words=%u actual_words=%u first_word=%u expected=%08x actual=%08x sdk=%u uniforms=%u cache=%u\n",expectedWords,slot.words,first,first<expectedWords?referenceOutput[first]:0,first<slot.words?slot.output[first]:0,picaNativeCompareCheckpoint(expectedCheckpoint,actualCheckpoint),!memcmp(&reference->uniforms,&executor.uniforms,sizeof(executor.uniforms)),!memcmp(&reference->state,&executor.state,sizeof(executor.state)));
                for(unsigned i=first>8?first-8:0;i<first+20 && (i<expectedWords || i<slot.words);++i)printf("PREPARED_COMPARE_WORD index=%u expected=%08x actual=%08x\n",i,i<expectedWords?referenceOutput[i]:0,i<slot.words?slot.output[i]:0);
                fflush(stdout);
            }
            if(!same){__atomic_store_n(&failure,1u,__ATOMIC_RELEASE);break;}
            // Validation keeps synchronous reference commands active on the GPU.
            memcpy(slot.output,referenceOutput,slot.words*sizeof(u32));
        }
        picaPreparedExecutor::Require(slot.words<=OutputWords && !(slot.words&1),"worker_output_capacity");
        if(worker)workerDraws+=executor.submitted-before;else mainDraws+=executor.submitted-before;
        last=&executor;
        __atomic_store_n(&completed,index+1u,__ATOMIC_SEQ_CST);
    }
    __atomic_store_n(&owner,0u,__ATOMIC_RELEASE);LightEvent_Signal(&finished);
}
void picaPreparedWorker::Commit()
{
    unsigned end=__atomic_load_n(&completed,__ATOMIC_SEQ_CST);
    if(committed==end)return;
    u64 start=svcGetSystemTick();
    while(committed!=end){
        Slot& slot=slots[committed%2];
        picaPreparedExecutor::Require(liveOffset<=liveSize && slot.words<=liveSize-liveOffset,"worker_commit_capacity");
        memcpy(liveBuffer+liveOffset,slot.output,slot.words*sizeof(u32));liveOffset+=slot.words;
        reserved-=slot.bound;++committed;
    }
    commitTicks+=svcGetSystemTick()-start;
}
void picaPreparedWorker::CheckFailure()
{
    if(!__atomic_load_n(&failure,__ATOMIC_ACQUIRE))return;
    // std::exit from a worker did not return safely to the resident loader on
    // hardware. Join it, restore main-owned command globals, then fail on main.
    while(__atomic_load_n(&owner,__ATOMIC_ACQUIRE))LightEvent_Wait(&finished);
    __atomic_store_n(&quit,1u,__ATOMIC_RELEASE);LightEvent_Signal(&wake);
    threadJoin(thread,U64_MAX);threadFree(thread);thread=nullptr;
    gpuCmdBuf=liveBuffer;gpuCmdBufSize=liveSize;gpuCmdBufOffset=liveOffset;
    picaPreparedExecutor::Require(false,"worker_comparison_mismatch");
}
void picaPreparedWorker::Wait(unsigned target)
{
    u64 start=svcGetSystemTick();
    while(__atomic_load_n(&completed,__ATOMIC_SEQ_CST)!=target){
        CheckFailure();Drain(false);CheckFailure();
        if(__atomic_load_n(&completed,__ATOMIC_SEQ_CST)!=target)LightEvent_Wait(&finished);
    }
    // Completion publishes immutable output; owner release also protects SDK
    // cache transfer and global buffer restoration at a foreign-call fence.
    while(__atomic_load_n(&owner,__ATOMIC_ACQUIRE))LightEvent_Wait(&finished);
    CheckFailure();waitTicks+=svcGetSystemTick()-start;Commit();
}
void picaPreparedWorker::Seal()
{
    Slot& slot=slots[producer];if(!slot.executor->operations)return;
    slot.bound=slot.executor->operations*picaPreparedExecutor::DrawWordBound;reserved+=slot.bound;++jobs;
    unsigned previous=__atomic_fetch_add(&published,1u,__ATOMIC_SEQ_CST);
    if(__atomic_load_n(&completed,__ATOMIC_SEQ_CST)==previous)LightEvent_Signal(&wake);
    producer=(previous+1u)%2;
}
void picaPreparedWorker::Publish(picaPreparedExecutor*& executor,picaPreparedInputPool*& pool)
{
    picaPreparedExecutor::Require(active,"worker_publish_without_lease");CheckFailure();
    if(!executor->operations)return;
    Seal();Commit();
    unsigned target=__atomic_load_n(&published,__ATOMIC_RELAXED);
    if(target-committed==2)Wait(target);
    executor=slots[producer].executor;pool=slots[producer].pool;
    picaPreparedExecutor::Require(!executor->operations,"worker_reuse_before_completion");
    pool->Reset();
}
picaPreparedExecutor* picaPreparedWorker::Finish(picaPreparedExecutor*& executor,picaPreparedInputPool*& pool)
{
    picaPreparedExecutor::Require(active,"worker_finish_without_lease");
    Seal();Wait(__atomic_load_n(&published,__ATOMIC_RELAXED));
    gpuCmdBuf=liveBuffer;gpuCmdBufSize=liveSize;gpuCmdBufOffset=liveOffset;
    last->End();slots[0].executor->valid=slots[1].executor->valid=false;
    executor=slots[0].executor;pool=slots[0].pool;active=false;
    // Keep the primary cache ready for the next main-owned import.
    picaPreparedExecutor* result=last;
    // A late empty wake can still inspect sequence counters. Reset them only
    // under the compiler lease, with atomic stores matching its atomic reads.
    unsigned idle=0;
    while(!__atomic_compare_exchange_n(&owner,&idle,1u,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){idle=0;LightEvent_Wait(&finished);}
    __atomic_store_n(&published,0u,__ATOMIC_SEQ_CST);__atomic_store_n(&completed,0u,__ATOMIC_SEQ_CST);
    committed=0;producer=0;
    __atomic_store_n(&owner,0u,__ATOMIC_RELEASE);
    return result;
}
bool picaPreparedWorker::GeometryRoom(unsigned geometry)const
{
    unsigned bound=(geometry+1)*picaPreparedExecutor::DrawWordBound;
    return liveOffset<=liveSize && reserved<=liveSize-liveOffset && bound<=liveSize-liveOffset-reserved;
}
void picaPreparedWorker::Buffer(u32** data,u32* size,u32* offset)const
{*data=liveBuffer;*size=liveSize;*offset=liveOffset;}
