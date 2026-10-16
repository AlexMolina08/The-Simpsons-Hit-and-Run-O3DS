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

// Linear allocation failures, separate from the ordinary heap.
extern "C" unsigned g_old3dsLinearFailures;
extern "C" unsigned g_old3dsLinearFailureBytes;

#endif // SRR2_OLD_3DS
#endif // OLD3DS_DIAG_H
