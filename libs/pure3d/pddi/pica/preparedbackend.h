#ifndef PICA_PREPARED_BACKEND_H
#define PICA_PREPARED_BACKEND_H
// This prototype executes synchronously. Counters belong to main until the
// future batch ownership transition; they are not safe for concurrent writes.
struct picaPreparedBackendStats {
    unsigned materials,uniforms,geometryCaptures,skinCaptures,legacyMaterials;
    unsigned viewRecords,lightRecords,materialUniformRecords;
    unsigned batchFlushes,batchedDraws;unsigned long long batchTicks;
    unsigned workerDraws,mainDraws,workerJobs,comparisons,mismatches;
    unsigned workerAvailable,workerCore,initialAllowance,actualAllowance,mainFp,workerFp;
    unsigned long long workerWaitTicks,workerCommitTicks;
};
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
extern bool g_picaRetainedMaterialEnabled;
extern unsigned g_picaRetainedMaterialBuilds,g_picaRetainedMaterialReuses,g_picaRetainedMaterialFallbacks;
#endif
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
struct picaDeferredSkinStats {unsigned captures=0,recipes=0,computedBones=0,allocated=0,live=0,peak=0,allocationFailures=0,inputFallbacks=0,materialized=0;};
void picaDeferredSkinGetStats(picaDeferredSkinStats*);
extern bool g_picaDeferredSkinEnabled;
#endif
extern bool g_picaPreparedBackendEnabled;
extern unsigned g_picaPreparedExecution;
void picaPreparedBackendFence();
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
// Main/loader own this summary. A disabled benchmark arm may still owe a join
// for the preceding frame, so feature enablement alone is never a fence guard.
extern bool g_picaPreparedOwnershipPending;
#endif
inline void picaPreparedFenceIfOwned()
{
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(!g_picaPreparedOwnershipPending)return;
#endif
    picaPreparedBackendFence();
}
enum {PICA_PREPARED_INVALIDATE=1,PICA_PREPARED_ALPHA_TEST,PICA_PREPARED_BLEND,PICA_PREPARED_CULL,PICA_PREPARED_DEPTH,PICA_PREPARED_VIEWPORT,PICA_PREPARED_SCISSOR};
bool picaPreparedCaptureState(unsigned,unsigned=0,unsigned=0,unsigned=0,unsigned=0,unsigned=0,unsigned=0);
inline bool picaPreparedCaptureOrFence(unsigned kind,unsigned a=0,unsigned b=0,unsigned c=0,unsigned d=0,unsigned e=0,unsigned f=0)
{
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
    if(!g_picaPreparedBackendEnabled && !g_picaPreparedOwnershipPending)return false;
#endif
    if(picaPreparedCaptureState(kind,a,b,c,d,e,f))return true;
    picaPreparedFenceIfOwned();return false;
}
class picaPreparedImmediateScope {
    unsigned previous;
public:
    explicit picaPreparedImmediateScope(bool fence=true):previous(g_picaPreparedExecution){if(fence){picaPreparedFenceIfOwned();g_picaPreparedExecution=1;}}
    ~picaPreparedImmediateScope(){g_picaPreparedExecution=previous;}
};
void picaPreparedBackendGetStats(picaPreparedBackendStats*);
#ifdef SRR2_OLD3DS_FRAME_PIPELINE
extern bool g_picaFramePrefixEnabled;
extern bool g_picaUnexposedResourcePrep;
class pddiBaseShader;
bool picaPreparedShaderNeedsFence(const pddiBaseShader*);
float picaFrameProcessingTime();
float picaFrameDrawingTime();
#endif
#endif
