#ifndef PICA_OCCLUSION_METRICS_H
#define PICA_OCCLUSION_METRICS_H
#include <3ds.h>
#ifdef __cplusplus
extern "C" {
#endif
// 0 original; 1 conservative rejection; 2 observe without rejecting.
extern unsigned g_picaVisibilityMode;
typedef struct {
    unsigned tested,hidden,skipped,skippedVertices,rasterCalls,triangles,cells,resets;
    unsigned geometryBytes,geometryCount,foreignBarriers;
    unsigned rejected[12];
    u64 queryTicks,rasterTicks,hiddenDrawTicks;
} picaVisibilityStats;
void picaVisibilityGetStats(picaVisibilityStats* output);
void picaVisibilityReset(bool newFrame);
void picaVisibilityShutdown(void);
void picaVisibilityForeignState(void);
void picaVisibilityObserved(u64 ticks);
#ifdef __cplusplus
}
#endif
#endif
