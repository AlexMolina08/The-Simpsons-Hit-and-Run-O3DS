#include <main/old3dsmemory.h>
#ifdef SRR2_OLD3DS_MEMORY_DIAG
#include <3ds.h>
#include <malloc.h>
#include <reent.h>
namespace {
bool sampling=false;
unsigned sampleStride=1,unlocks=0,observations=0;
bool forceSample=false;
unsigned peak=0,arenaPeak=0,normalCalls=0,normalFailures=0;
unsigned linearMinimum=~0u,linearCalls=0,linearFailures=0;
unsigned phase=O3MemoryStartup;
unsigned phaseNormal[O3MemoryPhaseCount]={},phaseArena[O3MemoryPhaseCount]={};
unsigned phaseLinear[O3MemoryPhaseCount]={~0u,~0u,~0u,~0u,~0u,~0u};
unsigned phaseLinearCalls[O3MemoryPhaseCount]={};
void Record(void* result,bool requested)
{
    __atomic_fetch_add(&normalCalls,1u,__ATOMIC_RELAXED);
    if(!result && requested)__atomic_fetch_add(&normalFailures,1u,__ATOMIC_RELAXED);
}
}
extern "C" {
// This SDK's newlib updates both maxima on allocator arena growth. Reading
// these existing counters needs no free-list walk and cannot miss a transient
// arena peak between sampled mallinfo observations. Custom heaps/SVC mappings
// remain outside this allocator-owned commitment bound.
extern unsigned long __malloc_max_sbrked_mem;
extern unsigned long __malloc_max_total_mem;
void __malloc_lock(struct _reent*);
void __real___malloc_unlock(struct _reent*);
void __wrap___malloc_unlock(struct _reent* r)
{
    // Observe while newlib still owns its existing recursive allocator lock.
    // A second outer lock would invert ordering with internal allocator calls.
    // mallinfo recursively unlocks: sampling suppresses that nested observation.
    if(!sampling && (forceSample || (++unlocks%sampleStride)==0)){
        forceSample=false;++observations;
        sampling=true;const struct mallinfo info=mallinfo();
        if(unsigned(info.uordblks)>peak)peak=info.uordblks;
        if(unsigned(info.arena)>arenaPeak)arenaPeak=info.arena;
        const unsigned selected=__atomic_load_n(&phase,__ATOMIC_RELAXED);
        if(unsigned(info.uordblks)>phaseNormal[selected])phaseNormal[selected]=info.uordblks;
        if(unsigned(info.arena)>phaseArena[selected])phaseArena[selected]=info.arena;
        sampling=false;
    }
    __real___malloc_unlock(r);
}

void* __real__malloc_r(struct _reent*,size_t);
void* __real__calloc_r(struct _reent*,size_t,size_t);
void* __real__realloc_r(struct _reent*,void*,size_t);
void* __real__memalign_r(struct _reent*,size_t,size_t);
void __real__free_r(struct _reent*,void*);
void* __wrap__malloc_r(struct _reent* r,size_t size){void* p=__real__malloc_r(r,size);Record(p,size!=0);return p;}
void* __wrap__calloc_r(struct _reent* r,size_t count,size_t size){void* p=__real__calloc_r(r,count,size);Record(p,count && size);return p;}
void* __wrap__realloc_r(struct _reent* r,void* old,size_t size){void* p=__real__realloc_r(r,old,size);Record(p,size!=0);return p;}
void* __wrap__memalign_r(struct _reent* r,size_t alignment,size_t size){void* p=__real__memalign_r(r,alignment,size);Record(p,size!=0);return p;}
void __wrap__free_r(struct _reent* r,void* p){__real__free_r(r,p);}
}
void Old3dsMemoryLinear(unsigned freeBytes,bool failed)
{
    __malloc_lock(nullptr);++linearCalls;if(failed)++linearFailures;
    if(freeBytes<linearMinimum)linearMinimum=freeBytes;
    const unsigned selected=__atomic_load_n(&phase,__ATOMIC_RELAXED);
    ++phaseLinearCalls[selected];
    if(freeBytes<phaseLinear[selected])phaseLinear[selected]=freeBytes;
    __real___malloc_unlock(nullptr);
}
void Old3dsMemorySetStride(unsigned stride)
{
    __malloc_lock(nullptr);sampleStride=stride?stride:1;
    __real___malloc_unlock(nullptr);
}
void Old3dsMemorySetPhase(unsigned selected)
{
    if(selected>=O3MemoryPhaseCount)return;
    if(__atomic_exchange_n(&phase,selected,__ATOMIC_RELAXED)==selected)return;
    // Carry current live allocations into a new phase, even if it allocates
    // nothing. Never hold the normal allocator lock while taking the linear lock.
    __malloc_lock(nullptr);forceSample=true;
    __wrap___malloc_unlock(nullptr);
    Old3dsMemoryLinear(linearSpaceFree(),false);
}
void Old3dsMemoryDump(FILE* file)
{
    __malloc_lock(nullptr);
    const unsigned long sbrkPeak=__malloc_max_sbrked_mem;
    const unsigned long totalPeak=__malloc_max_total_mem;
    // This diagnostic observes newlib allocator unlock boundaries while the
    // existing recursive allocator lock still excludes other threads. It does
    // not account for custom suballocators or arbitrary svcControlMemory maps.
    fprintf(file,"NATIVE_MEMORY diagnostic=1 normal_used_peak=%u normal_arena_peak=%u normal_calls=%u normal_failures=%u linear_free_min=%u linear_calls=%u linear_failures=%u application_region=%lu application_free=%lu layout=%u coverage=newlib_unlocks_and_linear_wrappers normal_stride=%u normal_observations=%u normal_used_is_sampled_lower_bound=%u performance_capture=0\n",
        peak,arenaPeak,normalCalls,normalFailures,linearMinimum,linearCalls,linearFailures,
        (unsigned long)osGetMemRegionSize(MEMREGION_APPLICATION),(unsigned long)osGetMemRegionFree(MEMREGION_APPLICATION),osGetApplicationMemType(),sampleStride,observations,sampleStride>1);
    fprintf(file,"NATIVE_MEMORY_ALLOCATOR_MAXIMA newlib_sbrk_peak=%lu newlib_total_peak=%lu sampled_used_within_commitment=%u sampled_arena_within_commitment=%u at_dump_entry=1 heap_walk=0 coverage=newlib_commitment_not_custom_heaps_or_svc performance_capture=0\n",
        sbrkPeak,totalPeak,peak<=totalPeak,arenaPeak<=sbrkPeak);
    const char* names[]={"startup","loading","warmup","route","route_streaming","reload"};
    for(unsigned i=0;i<O3MemoryPhaseCount;++i)fprintf(file,
        "NATIVE_MEMORY_PHASE phase=%s normal_used_peak=%u normal_arena_peak=%u linear_free_min=%u linear_observations=%u boundary=main_frame_context_and_loading_queue performance_capture=0\n",
        names[i],phaseNormal[i],phaseArena[i],phaseLinearCalls[i]?phaseLinear[i]:0,phaseLinearCalls[i]);
    __real___malloc_unlock(nullptr);
}
#endif
