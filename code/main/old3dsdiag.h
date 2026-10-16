//=============================================================================
// Old 3DS performance instrumentation counters.
//
// Counters are plain unsigned increments on paths that already run every frame.
// They are read and reset once per diagnostic interval by Game::Run(), which is
// the same interval the existing OLD3DS_PROFILE line covers. Nothing here
// changes simulation, rendering, or frame pacing; the values only describe work
// that was already happening.
//
// Substep buckets are derived from the elapsed milliseconds that
// WorldPhysicsManager::Update() actually received, using the same threshold
// expression that selects the substep count. They are not a count of substeps
// executed.
//=============================================================================
#ifndef OLD3DS_DIAG_H
#define OLD3DS_DIAG_H

#ifdef SRR2_OLD_3DS

// Physics. Bucket indices: 0 = one substep, 1 = two, 2 = three or more.
extern "C" unsigned g_old3dsPhysicsFrames[3];
extern "C" unsigned g_old3dsPhysicsSubsteps;
extern "C" unsigned g_old3dsPhysicsAreas;
extern "C" unsigned g_old3dsCollisionPairs;

// Frame presentation.
extern "C" unsigned g_old3dsFrameBeginOk;
extern "C" unsigned g_old3dsFrameBeginBusy;
extern "C" unsigned g_old3dsFrameBeginNoDraw;
extern "C" unsigned g_old3dsFramePresented;

// Draw submission. Dropped counters must stay at zero: a non-zero value means
// geometry is silently missing rather than work being saved.
extern "C" unsigned g_old3dsDrawCalls;
extern "C" unsigned g_old3dsDrawVerts;
extern "C" unsigned g_old3dsDrawDroppedNoFrame;
extern "C" unsigned g_old3dsDrawDroppedNotDrawable;
extern "C" unsigned g_old3dsDrawDroppedNoRoom;
extern "C" unsigned g_old3dsCmdHighWater;
extern "C" unsigned g_old3dsCmdSize;

// Render sub-phases, in call order: 0 traversal/sort, 1 opaque, 2 translucent,
// 3 billboards, 4 sparkles, 5 guts. These are subsets of render_ms and do not
// sum to it; the remainder is untimed.
extern "C" double   g_old3dsRenderPhases[6];

// Phase 2 CPU elapsed ticks: coins, simple shadows, depth sort, entity display.
// Each scope takes two tick reads and snapshots the existing submitted-draw count.
// The parent phase includes this instrumentation and remaining untimed work.
extern "C" unsigned long long g_old3dsPhase2Ticks[4];
extern "C" unsigned g_old3dsPhase2Draws[4];
extern "C" unsigned g_old3dsPhase2Entities[2]; // opaque, translucent classification
extern "C" unsigned g_old3dsSimpleShadowCalls;
extern "C" unsigned g_old3dsSimpleShadowDrawn; // calls issuing at least one draw

#include <3ds.h>
class Old3dsRenderScope
{
public:
    explicit Old3dsRenderScope(unsigned slot) : slot(slot), draws(g_old3dsDrawCalls), tick(svcGetSystemTick()) {}
    ~Old3dsRenderScope()
    {
        g_old3dsPhase2Ticks[slot] += svcGetSystemTick() - tick;
        g_old3dsPhase2Draws[slot] += g_old3dsDrawCalls - draws;
    }
private:
    unsigned slot, draws;
    u64 tick;
};


// Focused phase-2 detail. Categories are semantic DSG families, not shader flags.
enum Old3dsCategory { O3_OTHER, O3_VEHICLE, O3_CHARACTER, O3_ANIM, O3_ANIMCOLL,
    O3_STATIC, O3_STATICPHYS, O3_DYNA, O3_ANIMDYNA, O3_STATEPROP, O3_PARTICLE, O3_CATEGORY_COUNT };
enum Old3dsOperation { O3_SUBMIT, O3_MATERIAL, O3_UNIFORMS, O3_VERTEXLOCK,
    O3_VERTEXFLUSH, O3_EXPAND, O3_INDEXSCAN, O3_SKIN, O3_STREAM, O3_VEHICLEPOSE, O3_CHARACTERPREP,
    O3_OPERATION_COUNT };
enum Old3dsCounter { O3_INDEXED, O3_ARRAY, O3_TRANSIENT, O3_EXPANDED_DRAW,
    O3_EXPAND_REBUILD, O3_INDEX_TESTED, O3_MV_UPLOAD, O3_MV_REUSE,
    O3_MATERIAL_PASS, O3_TEX_REQUEST, O3_TEX_CHANGE, O3_STATE_CHANGE,
    O3_VERTEX_RELOCK, O3_RESCAN_CANDIDATE, O3_DEFAULT_VERTS, O3_COUNTER_COUNT };
struct Old3dsEntityStats
{
    u64 ticks;
    unsigned calls, samples, draws, indexed, arrays, blended, alphaTested;
};
struct Old3dsOperationStats { u64 ticks; unsigned calls, samples, units; };
extern "C" Old3dsEntityStats g_old3dsEntities[O3_CATEGORY_COUNT];
extern "C" Old3dsOperationStats g_old3dsOperations[O3_OPERATION_COUNT];
extern "C" unsigned g_old3dsBackend[O3_COUNTER_COUNT];
extern "C" int g_old3dsActiveEntity;
extern "C" bool g_old3dsDrawBlended, g_old3dsDrawAlphaTested;
inline void Old3dsCount(unsigned counter, unsigned units = 1)
{
    if(g_old3dsActiveEntity >= 0) g_old3dsBackend[counter] += units;
}
// One entity in four, one backend operation in 32. Counts/units are unsampled.
// Nested operation times overlap and must never be added to the parent totals.
class Old3dsOperationScope
{
public:
    Old3dsOperationScope(unsigned operation, unsigned units = 1) : stats(0), tick(0)
    {
        if(g_old3dsActiveEntity < 0) return;
        stats = &g_old3dsOperations[operation];
        stats->units += units;
        if((++stats->calls & 31u) == 1u) { ++stats->samples; tick = svcGetSystemTick(); }
    }
    void Finish() { if(tick) { stats->ticks += svcGetSystemTick() - tick; tick = 0; } }
    ~Old3dsOperationScope() { Finish(); }
private:
    Old3dsOperationStats* stats;
    u64 tick;
};
class Old3dsEntityScope
{
public:
    explicit Old3dsEntityScope(unsigned category) : previous(g_old3dsActiveEntity), tick(0), draws(g_old3dsDrawCalls)
    {
        g_old3dsActiveEntity = category < O3_CATEGORY_COUNT ? category : O3_OTHER;
        stats = &g_old3dsEntities[g_old3dsActiveEntity];
        if((++stats->calls & 3u) == 1u) { ++stats->samples; tick = svcGetSystemTick(); }
    }
    ~Old3dsEntityScope()
    {
        if(tick) stats->ticks += svcGetSystemTick() - tick;
        stats->draws += g_old3dsDrawCalls - draws;
        g_old3dsActiveEntity = previous;
    }
private:
    int previous;
    u64 tick;
    unsigned draws;
    Old3dsEntityStats* stats;
};


// Animated-object correlation only: no bounds or visibility behavior changes.
struct Old3dsAnimStats
{
    unsigned long long uid;
    char name[32];
    unsigned calls, draws, particleCalls, translatedCalls, translatedDraws;
};
extern "C" Old3dsAnimStats g_old3dsAnimStats[8];
extern "C" unsigned g_old3dsAnimQueries[4]; // all, particles, translated, table overflow
extern "C" unsigned g_old3dsWeights[5]; // cumulative loaded vertices, rigid, zero w1/w2/w3
class Old3dsAnimScope
{
public:
    Old3dsAnimScope(unsigned long long uid, const char* name, bool particle, bool translated)
        : stats(0), draws(g_old3dsDrawCalls), translated(translated)
    {
        for(unsigned i = 0; i < 8; ++i)
        {
            if(!g_old3dsAnimStats[i].calls || g_old3dsAnimStats[i].uid == uid)
            {
                stats = &g_old3dsAnimStats[i];
                if(!stats->calls)
                {
                    stats->uid = uid;
                    unsigned j = 0;
                    if(name) for(; j < 31 && name[j]; ++j) stats->name[j] = name[j];
                    stats->name[j] = 0;
                }
                ++stats->calls;
                if(particle) ++stats->particleCalls;
                if(translated) ++stats->translatedCalls;
                break;
            }
        }
        if(!stats) ++g_old3dsAnimQueries[3];
    }
    ~Old3dsAnimScope()
    {
        if(stats)
        {
            const unsigned submitted = g_old3dsDrawCalls - draws;
            stats->draws += submitted;
            if(translated) stats->translatedDraws += submitted;
        }
    }
private:
    Old3dsAnimStats* stats;
    unsigned draws;
    bool translated;
};

// Linear allocation failures, separate from the ordinary heap.
extern "C" unsigned g_old3dsLinearFailures;
extern "C" unsigned g_old3dsLinearFailureBytes;

// Redundant collision-volume updates. A "pass" is one
// CollisionDetector::DetectCollision() call over one area's pair list. Within a
// pass the detector only reads volumes, so updating the same root volume more
// than once recomputes identical values.
//
// First and repeat updates are timed in separate buckets, one call in eight, so
// the recoverable time is measured rather than assumed. mean_first_us is the
// cost of a first update of a volume in a pass; mean_red_us is the cost of a
// repeat. Recoverable time per interval is redundant * mean_red_us.
extern "C" unsigned g_old3dsDetectPasses;
extern "C" unsigned g_old3dsPairUpdates;
extern "C" unsigned g_old3dsUpdateAllCalls;
extern "C" unsigned g_old3dsUpdateAllRedundant;
extern "C" unsigned g_old3dsUpdateAllStaticSkip;
extern "C" unsigned g_old3dsUpdateAllSamples;
extern "C" double   g_old3dsUpdateAllMs;
extern "C" unsigned g_old3dsUpdateAllRedundantSamples;
extern "C" double   g_old3dsUpdateAllRedundantMs;

// Verify build only. g_old3dsUpdateAllVerified counts repeats whose geometry
// was hashed before and after the repeat update; g_old3dsUpdateAllVerifyMismatch
// counts those where the hash changed. The cache is only sound if the mismatch
// count stays zero: a non-zero value means a repeat altered geometry the
// detector reads, so skipping it would change collision results.
extern "C" unsigned g_old3dsUpdateAllVerified;
extern "C" unsigned g_old3dsUpdateAllVerifyMismatch;

// Ground triangle intersection diagnostics.
// Tracks query rate, hits/misses, candidates examined, plane tests entered,
// worst-case candidates in a query, redundant loop checks avoided, and sampled time.
extern "C" unsigned g_old3dsTriQueries;
extern "C" unsigned g_old3dsTriHits;
extern "C" unsigned g_old3dsTriMisses;
extern "C" unsigned g_old3dsTriCandidates;
extern "C" unsigned g_old3dsTriPlaneTests;
extern "C" unsigned g_old3dsTriWorst;
extern "C" unsigned g_old3dsTriRedundantAvoided;
extern "C" unsigned g_old3dsTriSamples;
extern "C" double   g_old3dsTriMs;

class Old3dsTriScope
{
public:
    explicit Old3dsTriScope(bool& found)
        : mFound(found),
          mCandidatesStart(g_old3dsTriCandidates),
          mTimed((++g_old3dsTriQueries & 31u) == 1u),
          mStartTick(mTimed ? svcGetSystemTick() : 0)
    {
    }
    ~Old3dsTriScope()
    {
        const unsigned queryCandidates = g_old3dsTriCandidates - mCandidatesStart;
        if(queryCandidates > g_old3dsTriWorst)
        {
            g_old3dsTriWorst = queryCandidates;
        }
        if(mFound)
        {
            ++g_old3dsTriHits;
        }
        else
        {
            ++g_old3dsTriMisses;
        }
        if(mTimed)
        {
            ++g_old3dsTriSamples;
            g_old3dsTriMs += (svcGetSystemTick() - mStartTick) * 1000.0 / SYSCLOCK_ARM11;
        }
    }
private:
    bool& mFound;
    unsigned mCandidatesStart;
    bool mTimed;
    u64 mStartTick;
};

#endif // SRR2_OLD_3DS
#endif // OLD3DS_DIAG_H
