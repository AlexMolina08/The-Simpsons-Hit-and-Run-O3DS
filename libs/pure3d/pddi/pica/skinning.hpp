#ifndef PICA_SKINNING_HPP
#define PICA_SKINNING_HPP
#include <pddi/pica/skincore.hpp>
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
#include <3ds.h>
struct picaSkinAllocation
{
    picaSkinAllocation *next;
    void *nativeBuffer;
    void *expandedBuffer;
    unsigned bytes;
    unsigned pedestrian; // Pool: 0 player, 1 pedestrian, 2 named NPC.
    unsigned char pad[32 - 3 * sizeof(void *) - 2 * sizeof(unsigned)];
};
static_assert(sizeof(picaSkinAllocation) == 32, "aligned skin allocation header");
struct picaSkinMetrics
{
    unsigned frame, frames, gpuGroups, gpuVertices, eligibleGroups, eligibleVertices;
    unsigned fallback[picaSkin::Count], switches, vectors, allocs, failures, frees;
    unsigned live, peak, minFree, maxCommand, mutations;
    u64 paletteTicks, avoidedWriteBytes, avoidedFlushBytes, workTicks, updateTicks, renderTicks;
    unsigned gameplayFrames, draws, submittedVertices, cpuReferenceVertices;
    u64 cpuReferenceTicks;
    unsigned mode;
};
extern picaSkinMetrics g_picaSkinMetrics;
extern picaSkin::Reason g_picaSkinAllocationReason;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
extern unsigned g_picaSkinBudgetFallbackVertices;
void picaSkinRecordRejection(const char* name, unsigned group, unsigned vertices);
#endif
bool picaSkinEnabled();
void picaSkinBeginFrame();
void picaSkinEndFrame(u64 work, u64 update, u64 render, bool gameplay);
void picaSkinRetire(picaSkinAllocation *p);
picaSkinAllocation *picaSkinAllocate(unsigned bytes, bool pedestrian = false, bool named = false);
extern "C" void picaSkinDump();
#endif
#endif
