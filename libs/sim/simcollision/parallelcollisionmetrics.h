#ifndef OLD3DS_PARALLEL_COLLISION_METRICS_H
#define OLD3DS_PARALLEL_COLLISION_METRICS_H
#include <3ds.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
// 0 original; 1 parallel; 2 compare with original before solver mutation.
extern unsigned g_old3dsParallelCollisionMode;
typedef struct {
    unsigned calls,areas,pairs,workerAreas,mainAreas,cancelled,overflows,fallbacks;
    unsigned verifiedAreas,verifiedPairs,verifiedContacts,mismatches,volumeMismatches;
    unsigned initialAllowance,actualAllowance,workerCore;
    u32 initResult,threadFailed;
    unsigned roots,workerGroups,mainGroups;
    u64 auditTicks,rootTicks;
    u64 prepareTicks,executeTicks,joinTicks,totalTicks;
} Old3dsParallelCollisionStats;
void old3dsParallelCollisionGetStats(Old3dsParallelCollisionStats* output);
void old3dsParallelCollisionDump(FILE* file);
#ifdef __cplusplus
}
#endif
#endif
