// Bounded raw interval storage. No formatting or I/O on the capture path.
#ifndef OLD3DS_REPORT_H
#define OLD3DS_REPORT_H
#include <main/old3dsring.h>
struct Old3dsReport
{
    Old3dsAnimStats anim[8];
    unsigned animQueries[4], weights[5];
    int ctx;
    unsigned mFrameCount;
    unsigned profileFrames;
    double updateMs;
    double renderMs;
    double serviceMs;
    double loadMs;
    double workMs;
    double peakWorkMs;
    unsigned overBudgetFrames;
    double fps;
    int paused;
    unsigned long linearFree;
    int heapUsed;
    float cpuMs;
    float gpuMs;
    unsigned g_old3dsBackend[O3_COUNTER_COUNT];
    unsigned g_old3dsCmdHighWater;
    unsigned g_old3dsCmdSize;
    unsigned g_old3dsCollisionPairs;
    unsigned g_old3dsDetectPasses;
    unsigned g_old3dsDrawCalls;
    unsigned g_old3dsDrawDroppedNoFrame;
    unsigned g_old3dsDrawDroppedNoRoom;
    unsigned g_old3dsDrawDroppedNotDrawable;
    unsigned g_old3dsDrawVerts;
    Old3dsEntityStats g_old3dsEntities[O3_CATEGORY_COUNT];
    unsigned g_old3dsFrameBeginBusy;
    unsigned g_old3dsFrameBeginNoDraw;
    unsigned g_old3dsFrameBeginOk;
    unsigned g_old3dsFramePresented;
    unsigned g_old3dsLinearFailureBytes;
    unsigned g_old3dsLinearFailures;
    unsigned g_old3dsModelViewReuses;
    unsigned g_old3dsModelViewUploads;
    Old3dsOperationStats g_old3dsOperations[O3_OPERATION_COUNT];
    unsigned g_old3dsPairUpdates;
    unsigned g_old3dsPhase2Draws[4];
    unsigned g_old3dsPhase2Entities[2];
    unsigned long long g_old3dsPhase2Ticks[4];
    unsigned g_old3dsPhysicsAreas;
    unsigned g_old3dsPhysicsFrames[3];
    unsigned g_old3dsPhysicsSubsteps;
    double g_old3dsRenderPhases[6];
    unsigned g_old3dsSimpleShadowCalls;
    unsigned g_old3dsSimpleShadowDrawn;
    unsigned g_old3dsUpdateAllCalls;
    double g_old3dsUpdateAllMs;
    unsigned g_old3dsUpdateAllRedundant;
    double g_old3dsUpdateAllRedundantMs;
    unsigned g_old3dsUpdateAllRedundantSamples;
    unsigned g_old3dsUpdateAllSamples;
    unsigned g_old3dsUpdateAllStaticSkip;
    unsigned g_old3dsUpdateAllVerified;
    unsigned g_old3dsUpdateAllVerifyMismatch;
    double g_old3dsUpdateStages[4];
    double g_picaFrameSubmitMs;
    double g_picaFrameWaitMs;
    unsigned g_old3dsTriQueries;
    unsigned g_old3dsTriHits;
    unsigned g_old3dsTriMisses;
    unsigned g_old3dsTriCandidates;
    unsigned g_old3dsTriPlaneTests;
    unsigned g_old3dsTriWorst;
    unsigned g_old3dsTriRedundantAvoided;
    unsigned g_old3dsTriSamples;
    double   g_old3dsTriMs;
};
static Old3dsRing<Old3dsReport, 64> s_old3dsReports;
static_assert(sizeof(s_old3dsReports) <= 128 * 1024, "Telemetry exceeds Old 3DS RAM budget");
static u64 s_old3dsCaptureTicks = 0;
static unsigned s_old3dsCaptures = 0;
static void Old3dsWriteReport(const Old3dsReport& record)
{
    printf("OLD3DS_ANIMQUERY all=%u particle=%u translated=%u overflow=%u\n",
        record.animQueries[0], record.animQueries[1], record.animQueries[2], record.animQueries[3]);
    for(unsigned i = 0; i < 8; ++i)
    {
        const Old3dsAnimStats& a = record.anim[i];
        if(a.calls) printf("OLD3DS_ANIM uid=%llu name=%s calls=%u draws=%u particle_calls=%u translated_calls=%u translated_draws=%u\n",
            a.uid, a.name, a.calls, a.draws, a.particleCalls, a.translatedCalls, a.translatedDraws);
    }
    printf("OLD3DS_WEIGHTS loaded=%u rigid=%u second_zero=%u third_zero=%u fourth_zero=%u\n",
        record.weights[0], record.weights[1], record.weights[2], record.weights[3], record.weights[4]);
    const auto& ctx = record.ctx;
    const auto& mFrameCount = record.mFrameCount;
    const auto& profileFrames = record.profileFrames;
    const auto& updateMs = record.updateMs;
    const auto& renderMs = record.renderMs;
    const auto& serviceMs = record.serviceMs;
    const auto& loadMs = record.loadMs;
    const auto& workMs = record.workMs;
    const auto& peakWorkMs = record.peakWorkMs;
    const auto& overBudgetFrames = record.overBudgetFrames;
    const auto& fps = record.fps;
    const auto& paused = record.paused;
    const auto& linearFree = record.linearFree;
    const auto& heapUsed = record.heapUsed;
    const auto& cpuMs = record.cpuMs;
    const auto& gpuMs = record.gpuMs;
    const auto& g_old3dsBackend = record.g_old3dsBackend;
    const auto& g_old3dsCmdHighWater = record.g_old3dsCmdHighWater;
    const auto& g_old3dsCmdSize = record.g_old3dsCmdSize;
    const auto& g_old3dsCollisionPairs = record.g_old3dsCollisionPairs;
    const auto& g_old3dsDetectPasses = record.g_old3dsDetectPasses;
    const auto& g_old3dsDrawCalls = record.g_old3dsDrawCalls;
    const auto& g_old3dsDrawDroppedNoFrame = record.g_old3dsDrawDroppedNoFrame;
    const auto& g_old3dsDrawDroppedNoRoom = record.g_old3dsDrawDroppedNoRoom;
    const auto& g_old3dsDrawDroppedNotDrawable = record.g_old3dsDrawDroppedNotDrawable;
    const auto& g_old3dsDrawVerts = record.g_old3dsDrawVerts;
    const auto& g_old3dsEntities = record.g_old3dsEntities;
    const auto& g_old3dsFrameBeginBusy = record.g_old3dsFrameBeginBusy;
    const auto& g_old3dsFrameBeginNoDraw = record.g_old3dsFrameBeginNoDraw;
    const auto& g_old3dsFrameBeginOk = record.g_old3dsFrameBeginOk;
    const auto& g_old3dsFramePresented = record.g_old3dsFramePresented;
    const auto& g_old3dsLinearFailureBytes = record.g_old3dsLinearFailureBytes;
    const auto& g_old3dsLinearFailures = record.g_old3dsLinearFailures;
    const auto& g_old3dsModelViewReuses = record.g_old3dsModelViewReuses;
    const auto& g_old3dsModelViewUploads = record.g_old3dsModelViewUploads;
    const auto& g_old3dsOperations = record.g_old3dsOperations;
    const auto& g_old3dsPairUpdates = record.g_old3dsPairUpdates;
    const auto& g_old3dsPhase2Draws = record.g_old3dsPhase2Draws;
    const auto& g_old3dsPhase2Entities = record.g_old3dsPhase2Entities;
    const auto& g_old3dsPhase2Ticks = record.g_old3dsPhase2Ticks;
    const auto& g_old3dsPhysicsAreas = record.g_old3dsPhysicsAreas;
    const auto& g_old3dsPhysicsFrames = record.g_old3dsPhysicsFrames;
    const auto& g_old3dsPhysicsSubsteps = record.g_old3dsPhysicsSubsteps;
    const auto& g_old3dsRenderPhases = record.g_old3dsRenderPhases;
    const auto& g_old3dsSimpleShadowCalls = record.g_old3dsSimpleShadowCalls;
    const auto& g_old3dsSimpleShadowDrawn = record.g_old3dsSimpleShadowDrawn;
    const auto& g_old3dsUpdateAllCalls = record.g_old3dsUpdateAllCalls;
    const auto& g_old3dsUpdateAllMs = record.g_old3dsUpdateAllMs;
    const auto& g_old3dsUpdateAllRedundant = record.g_old3dsUpdateAllRedundant;
    const auto& g_old3dsUpdateAllRedundantMs = record.g_old3dsUpdateAllRedundantMs;
    const auto& g_old3dsUpdateAllRedundantSamples = record.g_old3dsUpdateAllRedundantSamples;
    const auto& g_old3dsUpdateAllSamples = record.g_old3dsUpdateAllSamples;
    const auto& g_old3dsUpdateAllStaticSkip = record.g_old3dsUpdateAllStaticSkip;
    const auto& g_old3dsUpdateAllVerified = record.g_old3dsUpdateAllVerified;
    const auto& g_old3dsUpdateAllVerifyMismatch = record.g_old3dsUpdateAllVerifyMismatch;
    const auto& g_old3dsUpdateStages = record.g_old3dsUpdateStages;
    const auto& g_picaFrameSubmitMs = record.g_picaFrameSubmitMs;
    const auto& g_picaFrameWaitMs = record.g_picaFrameWaitMs;
            printf("OLD3DS_FRAME context=%d frame=%u fps=%.2f paused=%d linear_free=%lu heap_used=%d cpu_ms=%.2f gpu_ms=%.2f\n",
                (int)ctx, mFrameCount, fps,
                paused, linearFree, heapUsed,
                cpuMs, gpuMs);
            if(profileFrames)
                printf("OLD3DS_PROFILE samples=%u update_ms=%.2f render_ms=%.2f service_ms=%.2f load_ms=%.2f gpu_wait_ms=%.2f submit_ms=%.2f work_ms=%.2f peak_ms=%.2f over_budget=%u\n",
                    profileFrames, updateMs / profileFrames, renderMs / profileFrames,
                    serviceMs / profileFrames, loadMs / profileFrames,
                    g_picaFrameWaitMs / profileFrames, g_picaFrameSubmitMs / profileFrames,
                    workMs / profileFrames, peakWorkMs, overBudgetFrames);
            if(profileFrames)
                printf("OLD3DS_SIM samples=%u physics_ms=%.2f pedestrians_ms=%.2f traffic_ms=%.2f effects_ms=%.2f\n",
                    profileFrames, g_old3dsUpdateStages[0] / profileFrames,
                    g_old3dsUpdateStages[1] / profileFrames,
                    g_old3dsUpdateStages[2] / profileFrames,
                    g_old3dsUpdateStages[3] / profileFrames);
            printf("OLD3DS_RENDER mv_uploads=%u mv_reuses=%u\n",
                g_old3dsModelViewUploads, g_old3dsModelViewReuses);

            // Substeps are bucketed by the elapsed milliseconds the physics
            // update received, using the same threshold that selects the count.
            // This is the distribution of the selected count, not a count of
            // substeps executed.
            printf("OLD3DS_PHYS substep_frames_1=%u substep_frames_2=%u substep_frames_3plus=%u"
                   " substeps_total=%u areas=%u pairs=%u\n",
                g_old3dsPhysicsFrames[0], g_old3dsPhysicsFrames[1], g_old3dsPhysicsFrames[2],
                g_old3dsPhysicsSubsteps, g_old3dsPhysicsAreas, g_old3dsCollisionPairs);

            // begin_ok + begin_busy should equal the loops that attempted a
            // frame; presented counts frames actually submitted to the GPU.
            printf("OLD3DS_PRESENT begin_ok=%u begin_busy=%u begin_nodraw=%u presented=%u\n",
                g_old3dsFrameBeginOk, g_old3dsFrameBeginBusy,
                g_old3dsFrameBeginNoDraw, g_old3dsFramePresented);

            // dropped_* must stay at zero. Any non-zero value means geometry was
            // silently omitted, which would inflate FPS without being an
            // improvement. cmd_high_water is bytes used against cmd_size.
            printf("OLD3DS_DRAW calls=%u verts=%u dropped_noframe=%u dropped_notdrawable=%u"
                   " dropped_noroom=%u cmd_high_water=%u cmd_size=%u\n",
                g_old3dsDrawCalls, g_old3dsDrawVerts,
                g_old3dsDrawDroppedNoFrame, g_old3dsDrawDroppedNotDrawable,
                g_old3dsDrawDroppedNoRoom, g_old3dsCmdHighWater, g_old3dsCmdSize);

            // Slots: 0 traversal/sort, 1 opaque, 2 translucent, 3 billboards,
            // 4 sparkles, 5 guts. These are subsets of render_ms.
            if(profileFrames)
                printf("OLD3DS_RPHASE traversal_ms=%.2f opaque_ms=%.2f translucent_ms=%.2f"
                       " billboards_ms=%.2f sparkles_ms=%.2f guts_ms=%.2f\n",
                    g_old3dsRenderPhases[0] / profileFrames, g_old3dsRenderPhases[1] / profileFrames,
                    g_old3dsRenderPhases[2] / profileFrames, g_old3dsRenderPhases[3] / profileFrames,
                    g_old3dsRenderPhases[4] / profileFrames, g_old3dsRenderPhases[5] / profileFrames);

            if(profileFrames)
            {
                double parts[4], sum = 0.0;
                for(unsigned i = 0; i < 4; ++i)
                {
                    parts[i] = g_old3dsPhase2Ticks[i] * 1000.0 / SYSCLOCK_ARM11 / profileFrames;
                    sum += parts[i];
                }
                printf("OLD3DS_PHASE2 samples=%u coins_ms=%.4f shadows_ms=%.4f sort_ms=%.4f display_ms=%.4f remainder_ms=%.4f"
                       " coins_draws=%u shadows_draws=%u sort_draws=%u display_draws=%u opaque=%u translucent=%u shadow_calls=%u shadow_drawn=%u\n",
                    profileFrames, parts[0], parts[1], parts[2], parts[3],
                    g_old3dsRenderPhases[2] / profileFrames - sum,
                    g_old3dsPhase2Draws[0], g_old3dsPhase2Draws[1], g_old3dsPhase2Draws[2], g_old3dsPhase2Draws[3],
                    g_old3dsPhase2Entities[0], g_old3dsPhase2Entities[1],
                    g_old3dsSimpleShadowCalls, g_old3dsSimpleShadowDrawn);
            }

            if(profileFrames)
            {
                static const char* categories[] = {"other","vehicle","character","animated","animcollision",
                    "static","staticphys","dynamic","animdynamic","stateprop","particle"};
                static const char* operations[] = {"submit","material","uniforms","vertexlock","vertexflush",
                    "expand","indexscan","skin","stream","vehiclepose","characterprep"};
                for(unsigned i = 0; i < O3_CATEGORY_COUNT; ++i)
                {
                    const Old3dsEntityStats& e = g_old3dsEntities[i];
                    if(!e.calls) continue;
                    const double measuredMs = e.ticks * 1000.0 / SYSCLOCK_ARM11;
                    printf("OLD3DS_ENTITY category=%s calls=%u samples=%u sampled_ms=%.4f estimated_ms_per_frame=%.4f"
                           " draws=%u indexed=%u arrays=%u blended=%u alphatest=%u frames=%u\n",
                        categories[i], e.calls, e.samples, measuredMs,
                        e.samples ? measuredMs * e.calls / e.samples / profileFrames : 0.0,
                        e.draws, e.indexed, e.arrays, e.blended, e.alphaTested, profileFrames);
                }
                for(unsigned i = 0; i < O3_OPERATION_COUNT; ++i)
                {
                    const Old3dsOperationStats& o = g_old3dsOperations[i];
                    if(!o.calls) continue;
                    printf("OLD3DS_BACKEND op=%s calls=%u units=%u samples=%u sampled_ms=%.4f frames=%u\n",
                        operations[i], o.calls, o.units, o.samples, o.ticks * 1000.0 / SYSCLOCK_ARM11, profileFrames);
                }
                printf("OLD3DS_BCOUNTS indexed=%u arrays=%u transient=%u expanded_draw=%u expand_rebuild=%u"
                       " index_tested=%u mv_upload=%u mv_reuse=%u material_pass=%u tex_request=%u tex_change=%u"
                       " state_change=%u vertex_relock=%u rescan_candidate=%u default_verts=%u frames=%u\n",
                    g_old3dsBackend[0],g_old3dsBackend[1],g_old3dsBackend[2],g_old3dsBackend[3],
                    g_old3dsBackend[4],g_old3dsBackend[5],g_old3dsBackend[6],g_old3dsBackend[7],
                    g_old3dsBackend[8],g_old3dsBackend[9],g_old3dsBackend[10],g_old3dsBackend[11],
                    g_old3dsBackend[12],g_old3dsBackend[13],g_old3dsBackend[14],profileFrames);
            }

            if(g_old3dsLinearFailures)
                printf("OLD3DS_LINEARFAIL count=%u largest_bytes=%u\n",
                    g_old3dsLinearFailures, g_old3dsLinearFailureBytes);

            // Redundant collision-volume updates. calls is every UpdateAll()
            // reached from the pair loop; redundant counts those that repeated a
            // volume already updated in the same pass. mean_first_us and
            // mean_red_us are measured separately on one call in eight. The
            // recoverable time per interval is redundant * mean_red_us, since a
            // repeat does the same work as the first update of that volume.
            printf("OLD3DS_COLL passes=%u pairs_updated=%u calls=%u redundant=%u"
                   " static_skip=%u first_samples=%u mean_first_us=%.2f"
                   " red_samples=%u mean_red_us=%.2f\n",
                g_old3dsDetectPasses, g_old3dsPairUpdates, g_old3dsUpdateAllCalls,
                g_old3dsUpdateAllRedundant, g_old3dsUpdateAllStaticSkip,
                g_old3dsUpdateAllSamples,
                g_old3dsUpdateAllSamples ? (g_old3dsUpdateAllMs * 1000.0 / g_old3dsUpdateAllSamples) : 0.0,
                g_old3dsUpdateAllRedundantSamples,
                g_old3dsUpdateAllRedundantSamples ? (g_old3dsUpdateAllRedundantMs * 1000.0 / g_old3dsUpdateAllRedundantSamples) : 0.0);

#ifdef SRR2_OLD3DS_COLLVOL_VERIFY
            // Verify build: how many repeats were hashed before and after, and
            // how many of those changed geometry. mismatch must stay zero.
            printf("OLD3DS_COLLVERIFY verified=%u mismatch=%u\n",
                g_old3dsUpdateAllVerified, g_old3dsUpdateAllVerifyMismatch);
#endif

            if(record.g_old3dsTriQueries)
                printf("OLD3DS_TRI queries=%u hits=%u misses=%u candidates=%u planetests=%u worst=%u"
                       " redundant_avoided=%u samples=%u sampled_ms=%.3f mean_query_us=%.2f\n",
                    record.g_old3dsTriQueries, record.g_old3dsTriHits, record.g_old3dsTriMisses,
                    record.g_old3dsTriCandidates, record.g_old3dsTriPlaneTests, record.g_old3dsTriWorst,
                    record.g_old3dsTriRedundantAvoided, record.g_old3dsTriSamples,
                    record.g_old3dsTriMs,
                    record.g_old3dsTriSamples ? (record.g_old3dsTriMs * 1000.0 / record.g_old3dsTriSamples) : 0.0);

}
// Called only from a settled pause or after Game::Run exits. The caller
// excludes only this deliberate diagnostic stop from its elapsed-time clock.
// A failed write retains the ring for a later retry; forced termination cannot
// recover RAM, so hardware tests must explicitly dump before closing.
static bool Old3dsDumpReports(const char* reason)
{
    if(!s_old3dsReports.Size()) return true;
    clearerr(stdout);
    const u64 begin = svcGetSystemTick();
    printf("OLD3DS_BUFFER reason=%s count=%u overwritten=%u capacity=64 bytes=%u capture_ms=%.4f captures=%u\n",
        reason, s_old3dsReports.Size(), s_old3dsReports.Overwritten(),
        (unsigned)sizeof(s_old3dsReports), s_old3dsCaptureTicks * 1000.0 / SYSCLOCK_ARM11, s_old3dsCaptures);
    for(unsigned i = 0; i < s_old3dsReports.Size(); ++i)
        Old3dsWriteReport(s_old3dsReports.At(i));
    const u64 formatted = svcGetSystemTick();
    const int result = fflush(stdout);
    const u64 flushed = svcGetSystemTick();
    const bool ok = result == 0 && !ferror(stdout);
    printf("OLD3DS_DUMP format_write_ms=%.3f flush_ms=%.3f ok=%d\n",
        (formatted - begin) * 1000.0 / SYSCLOCK_ARM11,
        (flushed - formatted) * 1000.0 / SYSCLOCK_ARM11, ok);
    if(fflush(stdout) == 0 && !ferror(stdout) && ok)
    {
        s_old3dsReports.Clear();
        s_old3dsCaptureTicks = 0;
        s_old3dsCaptures = 0;
        return true;
    }
    return false;
}
#endif
