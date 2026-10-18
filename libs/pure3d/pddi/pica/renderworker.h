#ifndef PICA_RENDER_WORKER_H
#define PICA_RENDER_WORKER_H
#include <3ds.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
// 0 original; 1 owned synchronous compiler; 2 compare before original commit;
// 3 ordered core-1 compiler. Benchmark selection 4 randomizes 0/3 per frame.
extern unsigned g_picaRenderWorkerMode;
typedef struct {
    unsigned records,rawRecords,notifications;
    unsigned contexts,raw,workerContexts,mainContexts,fences,capacityFences;
    unsigned comparisons,mismatches,peakSlots,initialAllowance,actualAllowance,workerCore;
    unsigned initResult,threadFailed;
    u64 waitTicks,commitTicks;
} picaRenderWorkerStats;
void picaRenderWorkerBeginFrame(void);
void picaRenderWorkerEndFrame(void);
void picaRenderWorkerFence(void);
void picaRenderWorkerShutdown(void);
bool picaRenderWorkerContext(void);
void picaRenderWorkerGetStats(picaRenderWorkerStats* output);
void picaRenderWorkerDump(FILE* file);
#ifdef __cplusplus
}
#endif
#endif
