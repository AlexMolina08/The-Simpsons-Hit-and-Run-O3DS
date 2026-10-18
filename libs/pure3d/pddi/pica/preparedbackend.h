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
extern bool g_picaPreparedBackendEnabled;
extern unsigned g_picaPreparedExecution;
void picaPreparedBackendFence();
enum {PICA_PREPARED_INVALIDATE=1,PICA_PREPARED_ALPHA_TEST,PICA_PREPARED_BLEND,PICA_PREPARED_CULL,PICA_PREPARED_DEPTH,PICA_PREPARED_VIEWPORT,PICA_PREPARED_SCISSOR};
bool picaPreparedCaptureState(unsigned,unsigned=0,unsigned=0,unsigned=0,unsigned=0,unsigned=0,unsigned=0);
class picaPreparedImmediateScope {
    unsigned previous;
public:
    picaPreparedImmediateScope():previous(g_picaPreparedExecution){picaPreparedBackendFence();g_picaPreparedExecution=1;}
    ~picaPreparedImmediateScope(){g_picaPreparedExecution=previous;}
};
void picaPreparedBackendGetStats(picaPreparedBackendStats*);
#endif
