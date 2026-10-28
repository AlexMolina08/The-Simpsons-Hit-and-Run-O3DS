#include <pddi/pica/skinning.hpp>
#include <main/old3dsprofile.h>
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
#include <citro3d.h>
#include <cstdio>
#include <cstring>
#ifdef SRR2_OLD3DS_SLERP_CACHE
extern "C" bool radmathSlerpCacheStats(unsigned* hits, unsigned* misses);
#endif
picaSkinMetrics g_picaSkinMetrics = {};
picaSkin::Reason g_picaSkinAllocationReason = picaSkin::Ready;
static picaSkinAllocation *retired;
static unsigned liveBytes, pedestrianBytes, namedBytes, peakBytes, frameNumber;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
unsigned g_picaSkinBudgetFallbackVertices;
static unsigned retiredBytes, rejectionCount;
struct SkinRejection
{
    unsigned frame, bytes, pool, live, freeBytes, retired, flags, kind, group, vertices;
    char name[32];
};
static SkinRejection lastRejection, rejections[32];
static void budgetRejection(unsigned bytes, unsigned pool, unsigned freeBytes,
                            bool pedestrian, bool named, unsigned flags)
{
    lastRejection = {};
    lastRejection.frame=frameNumber;lastRejection.bytes=bytes;
    lastRejection.pool=pool;lastRejection.live=liveBytes;lastRejection.freeBytes=freeBytes;
    lastRejection.retired=retiredBytes;lastRejection.flags=flags;
    lastRejection.kind=named?2u:pedestrian?1u:0u;
}
void picaSkinRecordRejection(const char* name, unsigned group, unsigned vertices)
{
    if(g_picaSkinAllocationReason!=picaSkin::BudgetLimit)return;
    SkinRejection& record=rejections[rejectionCount++%32];record=lastRejection;
    record.group=group;record.vertices=vertices;
    if(name)std::snprintf(record.name,sizeof(record.name),"%s",name);
}
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_METRICS
static picaSkinMetrics records[256];
static unsigned recordCount, overwritten;
static float gpuTimes[256], cpuTimes[256];
#endif
bool picaSkinEnabled()
{
#ifdef SRR2_OLD3DS_GPU_SKIN
#ifdef SRR2_OLD3DS_GPU_SKIN_AB
    return (frameNumber / 120u) % 2u != 0;
#else
    return true;
#endif
#else
    return false;
#endif
}
picaSkinAllocation *picaSkinAllocate(unsigned bytes, bool pedestrian, bool named)
{
    g_picaSkinAllocationReason = picaSkin::BudgetLimit;
#ifndef SRR2_OLD3DS_GPU_SKIN_PEDESTRIANS
    if(pedestrian) return NULL;
#endif
#ifndef SRR2_OLD3DS_GPU_SKIN_CAST
    if(named) return NULL;
#endif
    if(pedestrian && named) return NULL;
    const unsigned poolLive = named ? namedBytes : pedestrian ? pedestrianBytes : liveBytes - pedestrianBytes - namedBytes;
    const unsigned freeBytes = linearSpaceFree();
    if (bytes > picaSkin::Budget - sizeof(picaSkinAllocation))
    {
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        budgetRejection(bytes,poolLive,freeBytes,pedestrian,named,1);
#endif
        return NULL;
    }
    bytes += sizeof(picaSkinAllocation);
#ifdef SRR2_OLD3DS_GPU_SKIN_BORROW
    const bool fits=picaSkin::FitsBorrowed(bytes,poolLive,liveBytes,freeBytes);
#else
    const bool fits=picaSkin::Fits(bytes,poolLive,freeBytes);
#endif
    if(!fits)
    {
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#ifdef SRR2_OLD3DS_GPU_SKIN_BORROW
        const unsigned poolLimit=picaSkin::BorrowPoolLimit, sharedLimit=picaSkin::SharedBudget;
#else
        const unsigned poolLimit=picaSkin::Budget, sharedLimit=~0u;
#endif
        const unsigned flags=(poolLive>poolLimit-bytes?2u:0u) |
            (liveBytes>sharedLimit-bytes?4u:0u) |
            (freeBytes<bytes || freeBytes-bytes<picaSkin::Reserve?8u:0u);
        budgetRejection(bytes,poolLive,freeBytes,pedestrian,named,flags);
#endif
        return NULL;
    }
    g_picaSkinAllocationReason = picaSkin::Allocation;
    picaSkinAllocation *p = (picaSkinAllocation *)linearAlloc(bytes);
    if (!p)
    {
        ++g_picaSkinMetrics.failures;
        return NULL;
    }
    p->bytes = bytes;
    p->pedestrian = named ? 2u : pedestrian ? 1u : 0u;
    p->next = NULL;
    p->nativeBuffer = NULL;
    p->expandedBuffer = NULL;
    liveBytes += bytes;
    if(pedestrian) pedestrianBytes += bytes;
    if(named) namedBytes += bytes;
    if (liveBytes > peakBytes)
        peakBytes = liveBytes;
    g_picaSkinAllocationReason = picaSkin::Ready;
    ++g_picaSkinMetrics.allocs;
    return p;
}
void picaSkinRetire(picaSkinAllocation *p)
{
    if (p)
    {
        p->next = retired;
        retired = p;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        retiredBytes+=p->bytes;
#endif
    }
}
void picaSkinBeginFrame()
{
    // Called only after successful blocking FrameBegin: previous GPU reads finished.
    while (retired)
    {
        picaSkinAllocation *p = retired;
        retired = p->next;
        liveBytes -= p->bytes;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        retiredBytes-=p->bytes;
#endif
        if(p->pedestrian == 1) pedestrianBytes -= p->bytes;
        if(p->pedestrian == 2) namedBytes -= p->bytes;
        if (p->nativeBuffer)
            linearFree(p->nativeBuffer);
        if (p->expandedBuffer)
            linearFree(p->expandedBuffer);
        linearFree(p);
        ++g_picaSkinMetrics.frees;
    }
    g_picaSkinMetrics.mode = picaSkinEnabled();
}
#ifdef SRR2_OLD3DS_GPU_SKIN_METRICS
static void capture()
{
    unsigned slot = recordCount++ % 256;
    if (recordCount > 256)
        ++overwritten;
    g_picaSkinMetrics.frame = frameNumber;
    g_picaSkinMetrics.live = liveBytes;
    g_picaSkinMetrics.peak = peakBytes;
    records[slot] = g_picaSkinMetrics;
    gpuTimes[slot] = C3D_GetDrawingTime();
    cpuTimes[slot] = C3D_GetProcessingTime();
    std::memset(&g_picaSkinMetrics, 0, sizeof(g_picaSkinMetrics));
}
#endif
void picaSkinEndFrame(u64 work, u64 update, u64 render, bool gameplay)
{
#ifdef SRR2_OLD3DS_RUNTIME_PROFILE
    g_picaSkinMetrics.workTicks += work;
    g_picaSkinMetrics.updateTicks += update;
    g_picaSkinMetrics.renderTicks += render;
    g_picaSkinMetrics.gameplayFrames += gameplay;
    ++g_picaSkinMetrics.frames;
    unsigned freeBytes = linearSpaceFree();
    if (!g_picaSkinMetrics.minFree || freeBytes < g_picaSkinMetrics.minFree)
        g_picaSkinMetrics.minFree = freeBytes;
#endif
    ++frameNumber;
#ifdef SRR2_OLD3DS_GPU_SKIN_METRICS
    if (frameNumber % 120u == 0)
        capture();
#endif
}
extern "C" void picaSkinDump()
{
    std::printf("GPU_SKIN_SUMMARY eligible_groups=%u gpu_groups=%u eligible_vertices=%u gpu_vertices=%u budget_fallbacks=%u alloc_failures=%u live=%u peak=%u pedestrians=%u named=%u minimum_linear_free=%u\n",
        g_picaSkinMetrics.eligibleGroups,g_picaSkinMetrics.gpuGroups,g_picaSkinMetrics.eligibleVertices,g_picaSkinMetrics.gpuVertices,
        g_picaSkinMetrics.fallback[picaSkin::BudgetLimit],g_picaSkinMetrics.failures,liveBytes,peakBytes,pedestrianBytes,namedBytes,g_picaSkinMetrics.minFree);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    std::printf("GPU_SKIN_REJECTIONS count=%u stored=%u flags_group=1 flags_pool=2 flags_shared=4 flags_reserve=8 fallback_vertices=%u\n",
                rejectionCount,rejectionCount<32?rejectionCount:32,g_picaSkinBudgetFallbackVertices);
    for(unsigned i=rejectionCount>32?rejectionCount-32:0;i<rejectionCount;++i)
    {
        const SkinRejection& r=rejections[i%32];
        std::printf("GPU_SKIN_REJECTION frame=%u name=%s group=%u vertices=%u kind=%u bytes=%u pool=%u live=%u retired=%u linear=%u flags=%u\n",
            r.frame,r.name,r.group,r.vertices,r.kind,r.bytes,r.pool,r.live,r.retired,r.freeBytes,r.flags);
    }
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_METRICS
    if (g_picaSkinMetrics.frames)
        capture();
    FILE *f = std::fopen("gpuskin.log", "w");
    if (!f)
        return;
    std::fprintf(
        f, "GPU_SKIN version=1 record_bytes=%u capacity=256 overwritten=%u budget=%u reserve=%u pedestrian_budget=%u named_budget=%u\n",
        (unsigned)sizeof(records), overwritten, picaSkin::Budget, picaSkin::Reserve,
#ifdef SRR2_OLD3DS_GPU_SKIN_PEDESTRIANS
        picaSkin::Budget,
#else
        0u,
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_CAST
        picaSkin::Budget);
#else
        0u);
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_BORROW
    std::fprintf(f, "GPU_SKIN_POOL shared=1 total_budget=%u pool_limit=%u group_limit=%u\n",
                 picaSkin::SharedBudget, picaSkin::BorrowPoolLimit, picaSkin::Budget);
#endif
#ifdef SRR2_OLD3DS_SLERP_CACHE
    unsigned slerpHits, slerpMisses;
    if(radmathSlerpCacheStats(&slerpHits, &slerpMisses))
        std::fprintf(f, "SLERP_CACHE hits=%u misses=%u\n", slerpHits, slerpMisses);
#endif
    unsigned begin = recordCount > 256 ? recordCount - 256 : 0;
    for (unsigned i = begin; i < recordCount; ++i)
    {
        unsigned slot = i % 256;
        const picaSkinMetrics &m = records[slot];
        std::fprintf(f,
                     "GPU_SKIN frame=%u frames=%u enabled=%u eligible_groups=%u "
                     "eligible_vertices=%u gpu_groups=%u gpu_vertices=%u palette_ms=%.6f "
                     "vectors=%u switches=%u avoided_write_bytes=%llu avoided_flush_bytes=%llu "
                     "allocs=%u failures=%u frees=%u live=%u peak=%u linear_min=%u command_high=%u "
                     "mutations=%u gpu_last_ms=%.4f cpu_last_ms=%.4f",
                     m.frame, m.frames, m.mode, m.eligibleGroups, m.eligibleVertices, m.gpuGroups,
                     m.gpuVertices, (double)m.paletteTicks * 1000.0 / SYSCLOCK_ARM11, m.vectors,
                     m.switches, (unsigned long long)m.avoidedWriteBytes,
                     (unsigned long long)m.avoidedFlushBytes, m.allocs, m.failures, m.frees, m.live,
                     m.peak, m.minFree, m.maxCommand, m.mutations, gpuTimes[slot], cpuTimes[slot]);
        std::fprintf(f, " work_ms=%.6f update_ms=%.6f render_ms=%.6f gameplay_frames=%u",
                     (double)m.workTicks * 1000.0 / SYSCLOCK_ARM11 / m.frames,
                     (double)m.updateTicks * 1000.0 / SYSCLOCK_ARM11 / m.frames,
                     (double)m.renderTicks * 1000.0 / SYSCLOCK_ARM11 / m.frames, m.gameplayFrames);
        std::fprintf(
            f, " draws=%u submitted_vertices=%u cpu_reference_vertices=%u cpu_reference_ms=%.6f",
            m.draws, m.submittedVertices, m.cpuReferenceVertices,
            (double)m.cpuReferenceTicks * 1000.0 / SYSCLOCK_ARM11);
        for (unsigned r = 0; r < picaSkin::Count; ++r)
            std::fprintf(f, " fallback_%u=%u", r, m.fallback[r]);
        std::fprintf(f, "\n");
    }
    std::fclose(f);
#endif
}
#endif
