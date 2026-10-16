#include <pddi/pica/skinning.hpp>
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
#include <citro3d.h>
#include <cstdio>
#include <cstring>
picaSkinMetrics g_picaSkinMetrics = {};
picaSkin::Reason g_picaSkinAllocationReason = picaSkin::Ready;
static picaSkinAllocation *retired;
static unsigned liveBytes, peakBytes, frameNumber;
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
picaSkinAllocation *picaSkinAllocate(unsigned bytes)
{
    g_picaSkinAllocationReason = picaSkin::BudgetLimit;
    if (bytes > picaSkin::Budget - sizeof(picaSkinAllocation))
        return NULL;
    bytes += sizeof(picaSkinAllocation);
    if (!picaSkin::Fits(bytes, liveBytes, linearSpaceFree()))
        return NULL;
    g_picaSkinAllocationReason = picaSkin::Allocation;
    picaSkinAllocation *p = (picaSkinAllocation *)linearAlloc(bytes);
    if (!p)
    {
        ++g_picaSkinMetrics.failures;
        return NULL;
    }
    p->bytes = bytes;
    p->next = NULL;
    p->nativeBuffer = NULL;
    p->expandedBuffer = NULL;
    liveBytes += bytes;
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
    g_picaSkinMetrics.workTicks += work;
    g_picaSkinMetrics.updateTicks += update;
    g_picaSkinMetrics.renderTicks += render;
    g_picaSkinMetrics.gameplayFrames += gameplay;
    ++frameNumber;
    ++g_picaSkinMetrics.frames;
    unsigned freeBytes = linearSpaceFree();
    if (!g_picaSkinMetrics.minFree || freeBytes < g_picaSkinMetrics.minFree)
        g_picaSkinMetrics.minFree = freeBytes;
#ifdef SRR2_OLD3DS_GPU_SKIN_METRICS
    if (frameNumber % 120u == 0)
        capture();
#endif
}
extern "C" void picaSkinDump()
{
#ifdef SRR2_OLD3DS_GPU_SKIN_METRICS
    if (g_picaSkinMetrics.frames)
        capture();
    FILE *f = std::fopen("gpuskin.log", "w");
    if (!f)
        return;
    std::fprintf(
        f, "GPU_SKIN version=1 record_bytes=%u capacity=256 overwritten=%u budget=%u reserve=%u\n",
        (unsigned)sizeof(records), overwritten, picaSkin::Budget, picaSkin::Reserve);
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
