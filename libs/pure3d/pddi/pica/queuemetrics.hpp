#ifndef PICA_QUEUE_METRICS_HPP
#define PICA_QUEUE_METRICS_HPP

#ifdef SRR2_OLD3DS_QUEUE_METRICS
#include <3ds.h>
struct picaQueueMetrics
{
    unsigned frames, geometries, candidates, queued, barriers, capacityFlushes;
    unsigned moved, originalChanges, submittedChanges, materialReuses;
    unsigned draws, vertices, materialPasses, textures, uniforms, uniformReuses;
    unsigned peakEntries, commandHighWater, commandSize;
    u64 geometryTicks, collectTicks, sortTicks, submitTicks, commandWords;
    unsigned geometrySamples, queueSamples;
    unsigned backendCalls, backendSamples, pipelineReuses, uniformBypasses;
    u64 backendTicks, opaqueTicks, translucentTicks, traversalTicks;
    unsigned queueMode;
};
extern picaQueueMetrics g_picaQueueMetrics;
class picaQueueTimer
{
public:
    picaQueueTimer(u64& total, bool enabled) : total(total), start(enabled ? svcGetSystemTick() : 0) {}
    ~picaQueueTimer() { if(start) total += svcGetSystemTick() - start; }
private:
    u64& total;
    u64 start;
};
class picaQueueBackendTimer
{
public:
    picaQueueBackendTimer() : start(0)
    {
        if((++g_picaQueueMetrics.backendCalls & 31u) == 1u)
        { ++g_picaQueueMetrics.backendSamples; start = svcGetSystemTick(); }
    }
    ~picaQueueBackendTimer() { if(start) g_picaQueueMetrics.backendTicks += svcGetSystemTick() - start; }
private:
    u64 start;
};
void picaQueueCapture();
extern "C" void picaQueueDump();
#endif
#endif
