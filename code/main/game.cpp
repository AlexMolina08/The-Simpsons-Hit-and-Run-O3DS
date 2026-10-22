#include <main/graphics3ds.h>
#include <main/old3dsperformance.h>
#ifdef SRR2_OLD_3DS
unsigned Old3dsPerformance::mode = 7;
#endif
#include <main/old3dsrecurring.h>
#include <main/old3dspools.h>
//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        game.cpp
//
// Description: The game loop
//
// History:     + Stolen and cleaned up from Penthouse -- Darwin Chau
//
//=============================================================================

//========================================
// System Includes
//========================================
// Standard Library
#include <stdlib.h>
#include <main/nativebench.h>
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
#include <pddi/pica/skinning.hpp>
#endif
#include <string.h>
// Foundation Tech
#include <raddebug.hpp>
#include <raddebugcommunication.hpp>
#include <raddebugconsole.hpp>
#include <raddebugwatch.hpp>
#include <radfile.hpp>
#include <radmemorymonitor.hpp>
#include <radtime.hpp>
// Pure3D
#include <p3d/loadmanager.hpp>
#include <p3d/utility.hpp>

#ifdef RAD_WIN32
#include <SDL.h>  // for SDL_PollEvent...
#endif

#ifdef RAD_3DS
#include <3ds.h>
#ifdef SRR2_OLD_3DS
#include <citro3d.h>
#include <malloc.h>
#include <main/old3dsdiag.h>
extern "C" unsigned g_picaPresentedFrames;
extern unsigned g_old3dsModelViewUploads;
extern unsigned g_old3dsModelViewReuses;
// Physics includes character simulation. The other counters are disjoint
// update stages; all are subsets of update_ms in OLD3DS_PROFILE.
double g_old3dsUpdateStages[4] = {};

// Old 3DS instrumentation counters. See code/main/old3dsdiag.h. Each is read
// and cleared once per diagnostic interval by Game::Run().
unsigned g_old3dsPhysicsFrames[3] = {};
unsigned g_old3dsPhysicsSubsteps = 0;
unsigned g_old3dsPhysicsAreas = 0;
unsigned g_old3dsCollisionPairs = 0;
unsigned g_old3dsFrameBeginOk = 0;
unsigned g_old3dsFrameBeginBusy = 0;
unsigned g_old3dsFrameBeginNoDraw = 0;
unsigned g_old3dsFramePresented = 0;
unsigned g_old3dsDrawCalls = 0;
unsigned g_old3dsDrawVerts = 0;
unsigned g_old3dsDrawDroppedNoFrame = 0;
unsigned g_old3dsDrawDroppedNotDrawable = 0;
unsigned g_old3dsDrawDroppedNoRoom = 0;
unsigned g_old3dsCmdHighWater = 0;
unsigned g_old3dsCmdSize = 0;
double   g_old3dsRenderPhases[6] = {};
unsigned long long g_old3dsPhase2Ticks[4] = {};
unsigned g_old3dsPhase2Draws[4] = {};
unsigned g_old3dsPhase2Entities[2] = {};
unsigned g_old3dsSimpleShadowCalls = 0;
unsigned g_old3dsSimpleShadowDrawn = 0;
Old3dsEntityStats g_old3dsEntities[O3_CATEGORY_COUNT] = {};
Old3dsOperationStats g_old3dsOperations[O3_OPERATION_COUNT] = {};
unsigned g_old3dsBackend[O3_COUNTER_COUNT] = {};
Old3dsAnimStats g_old3dsAnimStats[8] = {};
unsigned g_old3dsAnimQueries[4] = {};
unsigned g_old3dsWeights[5] = {};
int g_old3dsActiveEntity = -1;
bool g_old3dsDrawBlended = false, g_old3dsDrawAlphaTested = false;
unsigned g_old3dsLinearFailures = 0;
unsigned g_old3dsLinearFailureBytes = 0;
unsigned g_old3dsDetectPasses = 0;
unsigned g_old3dsPairUpdates = 0;
unsigned g_old3dsUpdateAllCalls = 0;
unsigned g_old3dsUpdateAllRedundant = 0;
unsigned g_old3dsUpdateAllStaticSkip = 0;
unsigned g_old3dsUpdateAllSamples = 0;
double   g_old3dsUpdateAllMs = 0.0;
unsigned g_old3dsUpdateAllRedundantSamples = 0;
double   g_old3dsUpdateAllRedundantMs = 0.0;
unsigned g_old3dsUpdateAllVerified = 0;
unsigned g_old3dsUpdateAllVerifyMismatch = 0;
unsigned g_old3dsTriQueries = 0;
unsigned g_old3dsTriHits = 0;
unsigned g_old3dsTriMisses = 0;
unsigned g_old3dsTriEmptyLeaf = 0;
unsigned g_old3dsTriCandidates = 0;
unsigned g_old3dsTriPlaneTests = 0;
unsigned g_old3dsTriWorst = 0;
unsigned g_old3dsTriExhausted = 0;
unsigned g_old3dsTriRescanAvoided = 0;
unsigned g_old3dsTriSamples = 0;
double   g_old3dsTriMs = 0.0;
extern "C" int g_picaWorldScaleAllowed;
extern "C" double g_picaFrameWaitMs;
extern "C" double g_picaFrameSubmitMs;
#endif

extern "C" int      srr2SuspendFiles( void );
extern "C" int      srr2ResumeFiles( void );
extern "C" int      g_picaMipmapsAllowed;
extern "C" unsigned g_p3dYieldTimeUs;
extern "C" bool old3dsNewspaperAnimating(void);

static void srr2AptHook( APT_HookType hook, void* )
{
    if( hook == APTHOOK_ONSUSPEND )
    {
        srr2SuspendFiles();
    }
    else if( hook == APTHOOK_ONRESTORE )
    {
        srr2ResumeFiles();
    }
}
#endif

//========================================
// Project Includes
//========================================
#include <contexts/contextenum.h>
#ifdef RAD_3DS
#include <camera/supercammanager.h>
#include <camera/supercam.h>
#endif
#include <debug/profiler.h>
#include <gameflow/gameflow.h>
#include <presentation/gui/ingame/guiscreenmissionload.h>
#include <main/commandlineoptions.h>
#include <main/game.h>
#include <main/platform.h>

#ifdef RAD_GAMECUBE
#include <main/gamecube_extras/gcmanager.h>
#endif

#include <memory/srrmemory.h>
#include <memory/memoryutilities.h>

#include <render/RenderFlow/renderflow.h>
#include <sound/soundmanager.h>
#include <input/inputmanager.h>

//******************************************************************************
//
// Global Data, Local Data, Local Classes
//
//******************************************************************************

//
// Static pointer to instance of this singleton.
//
Game* Game::spInstance = NULL;

bool g_inDemoMode = false;

//#define DEMO_MODE_PROFILER

#ifdef DEMO_MODE_PROFILER
    #define DEMOPROFILE(X) X
#else
    #define DEMOPROFILE(X)
#endif

#ifdef DEMO_MODE_PROFILER

#ifdef RAD_PS2
#include <libgraph.h>
#endif

DemoProfiler::DemoProfiler(unsigned mf) :
    recording(false), maxFrames(mf), nChannel(0), currentFrame(0),
    alertStatus(PROFILER_ALERT_GREEN),
    numFramesBelow_20(0), numFramesBetween_20_30(0),numFramesBetween_30_40(0), numFramesAbove_40(0)
{
    for( int i=0; i < MAX_CHANNEL; i++)
    {
        channel[i] = NULL;
    }
}
    
void DemoProfiler::AddChannel(unsigned c, const char* name)
{
    rReleaseAssert(c < MAX_CHANNEL);
    channel[c] = new Channel;
    channel[c]->samples = new unsigned[maxFrames];
    memset(channel[c]->samples, 0, maxFrames*sizeof(unsigned));
    channel[c]->t0 = 0;
    strncpy(channel[c]->name, name, 254);
    nChannel++;
}

void DemoProfiler::Start(unsigned c)
{
    if(!channel[c]) return;
    if( recording )
    {
        channel[c]->t0 = radTimeGetMicroseconds64();
    }
}


void DemoProfiler::Stop(unsigned c)
{
    if(!channel[c]) return;
    if( recording )
    {
        radTime64 elapsed = radTimeGetMicroseconds64() - channel[c]->t0;
        channel[c]->samples[currentFrame] += elapsed;

        if(c == 0)
        {
            alertStatus = PROFILER_ALERT_GREEN;
            if( elapsed >= 50000 )
            {
                numFramesBelow_20++;
                alertStatus = PROFILER_ALERT_RED;
            }
            else if( (elapsed < 50000) && (elapsed >= 33333) )
            {
                numFramesBetween_20_30++;
                alertStatus = PROFILER_ALERT_YELLOW;
            }
            else if( (elapsed < 33333) && (elapsed >= 25000) )
            {
                numFramesBetween_30_40++;
            }
            else
            {
                numFramesAbove_40++;
            }
        }
    }
}

void DemoProfiler::Set(unsigned c, unsigned val)
{
    if(!channel[c]) return;
    if( recording )
    {
        channel[c]->samples[currentFrame] = val;
    }
}

void DemoProfiler::StartRecording()
{
    recording = true;
}

bool DemoProfiler::IsRecording()
{
    return recording;
}

unsigned DemoProfiler::GetSample(unsigned c)
{
    return (recording && channel[c]) ? channel[c]->samples[currentFrame] : 0;
}

unsigned DemoProfiler::GetCurrentFrame()
{
    return currentFrame;
}

void DemoProfiler::Accumulate(unsigned c, unsigned val)
{
    if(!channel[c]) return;
    if( recording )
    {
        channel[c]->samples[currentFrame] += val;
    }
}

void DemoProfiler::NextFrame()
{
    if( recording )
    {
        currentFrame++;
        if(currentFrame >= maxFrames)
        {
            Dump();
            recording = false;
        }
    }
}

DemoProfiler::AlertStatus DemoProfiler::GetAlertStatus()
{
    return alertStatus;
}

void DemoProfiler::Dump()
{
    rReleasePrintf("\n\n~~~~~~~~~~~~~~~~~~~~~ PROFILER STATS ~~~~~~~~~~~~~~~~~~~~~\n");
    rReleasePrintf(    "Total Frames: %d\n"
                       "< 20 fps: %d (%.2f%%)\n"
                       "20-30 fps: %d (%.2f%%)\n"
                       "30-40 fps: %d (%.2f%%)\n"
                       ">40 fps: %d (%.2f%%)\n",
                       maxFrames,
                       numFramesBelow_20, 100.0f * (float)numFramesBelow_20 / (float)maxFrames,
                       numFramesBetween_20_30, 100.0f * (float)numFramesBetween_20_30 / (float)maxFrames,
                       numFramesBetween_30_40, 100.0f * (float)numFramesBetween_30_40 / (float)maxFrames,
                       numFramesAbove_40, 100.0f * (float)numFramesAbove_40 / (float)maxFrames );

    for( unsigned i=0; i < MAX_CHANNEL; i++)
    {
        if(channel[i])
        {
            rReleasePrintf("%s\t", channel[i]->name);
        }
    }
    rReleasePrintf("\n");

    for( unsigned i=0; i < maxFrames; i++ )
    {
        for( unsigned j=0; j < MAX_CHANNEL; j++ )
        {
            if(channel[j])
            {
                rReleasePrintf("%.1f\t", (float)channel[j]->samples[i] * 0.001f);
            }
        }
        rReleasePrintf("\n");

        if( !(i % 10) )
        {
            rmt::Sin(0.0f);
#ifdef RAD_PS2
            sceGsSyncV(0);
#endif
        }
    }

    while(1)
    {
        rmt::Sin(0.0f);
    }
}


DemoProfiler g_DemoProfiler( 1850 );  // about a minute at 30 fps

#include <pddi/pddi.hpp>

static bool g_DemoProfiler_Started = false;
static const int g_DemoProfiler_StartFrame = 150;
static int g_DemoProfiler_CurrentFrame = 0;

#endif


//******************************************************************************
//
// Public Member Functions
//
//******************************************************************************

//==============================================================================
// Game::CreateInstance
//==============================================================================
// Description: Create the game
//
// Parameters:	platform - the platform that the game is to be created on
//
// Return:      pointer to the created game
//
// Constraints: This is a singleton so only one instance is allowed.
//
//==============================================================================
Game* Game::CreateInstance( Platform* platform )
{
    rAssert( platform != NULL );

    rAssertMsg( (spInstance == NULL), "Trying to create more than one instance of the game!" );

MEMTRACK_PUSH_GROUP( "Game" );
    if( spInstance == NULL )
    {
        spInstance = new(GMA_PERSISTENT) Game( platform );
        rAssert( spInstance != NULL );
    }
MEMTRACK_POP_GROUP( "Game" );
    
    return spInstance;
}


//==============================================================================
// Game::DestroyInstance
//==============================================================================
// Description: Destroy the game
//
// Parameters:	None.
//
// Return:      None.
//
//==============================================================================
void Game::DestroyInstance()
{
    delete( GMA_PERSISTENT, spInstance );
    spInstance = NULL;
}


//==============================================================================
// Game::GetInstance
//==============================================================================
// Synopsis:    Get an instance of the game
//
// Parameters:  None.
//
// Returns:     a poitner to the game
//
// Constraints: Game must be created before this is called
//
//==============================================================================
Game* Game::GetInstance()
{
    rAssertMsg((spInstance != NULL), "Trying to get an instance of the game before it is created!");
    
    return spInstance;
}

//=============================================================================
// Game::GetPlatform
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      Platform
//
//=============================================================================
Platform* Game::GetPlatform()
{
    return mpPlatform;
}


//==============================================================================
// Game::Initialize
//==============================================================================
// Synopsis:    Initialize the game
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
void Game::Initialize()
{
    rAssert( mpPlatform != NULL );

    //
    // Initialize the platform and core systems.
    //
    mpPlatform->InitializePlatform();

    //
    // Initialize the sound manager.
    //
    SoundManager::GetInstance()->Initialize();
    
    //
    // Initialize the timer system
    //
    ::radTimeCreateList( &mpTimerList,
                         16, // Default
                         GMA_PERSISTENT );

    rAssert( mpTimerList != NULL );

    //
    // Create the GameFlow & Couple the RenderFlow
    //
    mpGameFlow = GameFlow::CreateInstance();
    mpRenderFlow = RenderFlow::GetInstance();
    mpRenderFlow->DoAllRegistration();

    CGuiScreenMissionLoad::InitializePermanentVariables();

#ifdef RAD_E3
    rReleasePrintf( "\n----------=[  SIMPSONS HIT & RUN - E3 BUILD  ]=----------\n\n" );
#endif

    //
    // Set the starting context
    //
    mpGameFlow->SetContext( CONTEXT_BOOTUP );
}


//==============================================================================
// Game::Terminate
//==============================================================================
// Synopsis:    Clean up and shut down.
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
void Game::Terminate() 
{
    rAssert( mpGameFlow != NULL );
    rAssert( mpRenderFlow != NULL );
    rAssert( mpTimerList != NULL );
    rAssert( mpPlatform != NULL );

    //
    // Kill the flow servers.
    //
    mpGameFlow->DestroyInstance();
    mpGameFlow = NULL;
    
    // Render flow destroyed by singletons.cpp
    //mpRenderFlow->DestroyInstance();
    mpRenderFlow = NULL;
    
    //
    // Release the game's references to the timer list.
    //
    mpTimerList->Release();
    mpTimerList = NULL;
}


//==============================================================================
// Game::Run
//==============================================================================
// Synopsis:    This is where game loop spins.  It exits after Stop() is called.
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
const unsigned PROFILE_CHANNEL_ALL = 0;
const unsigned PROFILE_CHANNEL_AI = 1;
const unsigned PROFILE_CHANNEL_RENDER = 2;
const unsigned PROFILE_CHANNEL_LOAD = 3;

#ifdef SRR2_OLD_3DS
#include <main/old3dsreport.h>
#endif

void Game::Run() 
{
#ifdef SRR2_OLD_3DS
    printf("OLD3DS_RUN enter\n");
#endif
    extern bool g_AllowDebugOutput;
    
#ifdef DEMO_MODE_PROFILER
    g_AllowDebugOutput = false;
    g_DemoProfiler.AddChannel(0, "All");
    g_DemoProfiler.AddChannel(1, "Ai");
    g_DemoProfiler.AddChannel(2, "Render");
    g_DemoProfiler.AddChannel(3, "Load");
    g_DemoProfiler.AddChannel(4, "Opaque");
    g_DemoProfiler.AddChannel(5, "Translucent");
    g_DemoProfiler.AddChannel(6, "World Sphere");
    g_DemoProfiler.AddChannel(7, "Anim Entity");
    g_DemoProfiler.AddChannel(8, "Breakable");
    g_DemoProfiler.AddChannel(9, "InstAnimDynaPhys");
    g_DemoProfiler.AddChannel(10, "InstDynaPhys");
    g_DemoProfiler.AddChannel(11, "InstStat");
    g_DemoProfiler.AddChannel(12, "InstStatPhys");
    g_DemoProfiler.AddChannel(13,"Lens Flare");
    g_DemoProfiler.AddChannel(14, "State Prop");
    g_DemoProfiler.AddChannel(15, "Static");
    g_DemoProfiler.AddChannel(16, "Tristrip");
    g_DemoProfiler.AddChannel(17, "AnimCollision");
    g_DemoProfiler.AddChannel(18, "Cars");
    g_DemoProfiler.AddChannel(19, "Characters");
    g_DemoProfiler.AddChannel(20, "ABH");
    g_DemoProfiler.AddChannel(21, "Actor");
    g_DemoProfiler.AddChannel(22, "AnimatedIcon");
    g_DemoProfiler.AddChannel(23, "ParticleSystem");

#endif

#ifdef RAD_3DS
    static aptHookCookie s_aptCookie;
    aptHook( &s_aptCookie, srr2AptHook, NULL );
#endif
#ifdef SRR2_OLD_3DS
    double updateMs = 0.0, renderMs = 0.0, serviceMs = 0.0, loadMs = 0.0;
    unsigned profileFrames = 0;
    double workMs = 0.0, peakWorkMs = 0.0;
    unsigned overBudgetFrames = 0;
#endif
    unsigned time = radTimeGetMilliseconds();
    while( !mExitNow )
    {
#ifdef SRR2_OLD_3DS
        if(mFrameCount == 0) printf("OLD3DS_RUN loop\n");
#endif
#ifdef RAD_3DS
        if( !aptMainLoop() )
        {
            mExitNow = true;
            break;
        }

        hidScanInput();
#ifdef SRR2_OLD_3DS
        if(mFrameCount == 0) printf("OLD3DS_RUN input\n");
#endif

        const ContextEnum ctx = GetGameFlow() ? GetGameFlow()->GetCurrentContext() : CONTEXT_ENTRY;
#ifdef SRR2_OLD_3DS
        static u64 lastDiagnosticTick = svcGetSystemTick();
        static unsigned lastPresented = g_picaPresentedFrames;
        // Pause first, then press SELECT to dump. Hold L as well to exit cleanly.
        // Never dump while simulation runs.
        #ifdef SRR2_OLD3DS_CAUSAL_DIAG
        if(ctx == CONTEXT_PAUSE && (hidKeysDown() & KEY_SELECT) && (hidKeysHeld() & KEY_R))
            O3ExperimentArm((hidKeysHeld() & KEY_X)!=0);
        else
#endif
        if(ctx == CONTEXT_PAUSE && (hidKeysDown() & KEY_SELECT))
        {
            const unsigned dumpStart = radTimeGetMilliseconds();
            const u64 dumpStartTick = svcGetSystemTick();
            const bool dumped = Old3dsDumpReports("pause-chord");
            time += radTimeGetMilliseconds() - dumpStart;
            lastDiagnosticTick += svcGetSystemTick() - dumpStartTick;
            if(dumped && (hidKeysHeld() & KEY_L))
            {
                Stop();
                break;
            }
        }
#endif
#ifdef SRR2_OLD_3DS
        #ifdef SRR2_OLD3DS_CAUSAL_DIAG
        // Begin after the interval capture resets its counters below.
#endif
        Graphics3DS::AllowChanges(ctx == CONTEXT_FRONTEND || ctx == CONTEXT_PAUSE);
        const u64 frameStartTick = svcGetSystemTick();
        const u64 diagnosticTick = svcGetSystemTick();
        const u64 sampleTicks = diagnosticTick - lastDiagnosticTick;
        if(sampleTicks >= (u64)SYSCLOCK_ARM11 * 5)
        {
            const u64 captureStart = svcGetSystemTick();
            const unsigned presented = g_picaPresentedFrames;
            Old3dsReport& record = s_old3dsReports.Push();
            record.ctx = (int)ctx;
            record.mFrameCount = mFrameCount;
            record.profileFrames = profileFrames;
            record.updateMs = updateMs;
            record.renderMs = renderMs;
            record.serviceMs = serviceMs;
            record.loadMs = loadMs;
            record.workMs = workMs;
            record.peakWorkMs = peakWorkMs;
            record.overBudgetFrames = overBudgetFrames;
            record.fps = (presented - lastPresented) * (double)SYSCLOCK_ARM11 / sampleTicks;
            record.paused = mpPlatform->PausedForErrors();
            record.linearFree = (unsigned long)linearSpaceFree();
            record.heapUsed = mallinfo().uordblks;
            record.cpuMs = C3D_GetProcessingTime();
            record.gpuMs = C3D_GetDrawingTime();
            memcpy(record.g_old3dsBackend, g_old3dsBackend, sizeof(g_old3dsBackend));
            record.g_old3dsCmdHighWater = g_old3dsCmdHighWater;
            record.g_old3dsCmdSize = g_old3dsCmdSize;
            record.g_old3dsCollisionPairs = g_old3dsCollisionPairs;
            record.g_old3dsDetectPasses = g_old3dsDetectPasses;
            record.g_old3dsDrawCalls = g_old3dsDrawCalls;
            record.g_old3dsDrawDroppedNoFrame = g_old3dsDrawDroppedNoFrame;
            record.g_old3dsDrawDroppedNoRoom = g_old3dsDrawDroppedNoRoom;
            record.g_old3dsDrawDroppedNotDrawable = g_old3dsDrawDroppedNotDrawable;
            record.g_old3dsDrawVerts = g_old3dsDrawVerts;
            memcpy(record.g_old3dsEntities, g_old3dsEntities, sizeof(g_old3dsEntities));
            record.g_old3dsFrameBeginBusy = g_old3dsFrameBeginBusy;
            record.g_old3dsFrameBeginNoDraw = g_old3dsFrameBeginNoDraw;
            record.g_old3dsFrameBeginOk = g_old3dsFrameBeginOk;
            record.g_old3dsFramePresented = g_old3dsFramePresented;
            record.g_old3dsLinearFailureBytes = g_old3dsLinearFailureBytes;
            record.g_old3dsLinearFailures = g_old3dsLinearFailures;
            record.g_old3dsModelViewReuses = g_old3dsModelViewReuses;
            record.g_old3dsModelViewUploads = g_old3dsModelViewUploads;
            memcpy(record.g_old3dsOperations, g_old3dsOperations, sizeof(g_old3dsOperations));
            record.g_old3dsPairUpdates = g_old3dsPairUpdates;
            memcpy(record.g_old3dsPhase2Draws, g_old3dsPhase2Draws, sizeof(g_old3dsPhase2Draws));
            memcpy(record.g_old3dsPhase2Entities, g_old3dsPhase2Entities, sizeof(g_old3dsPhase2Entities));
            memcpy(record.g_old3dsPhase2Ticks, g_old3dsPhase2Ticks, sizeof(g_old3dsPhase2Ticks));
            record.g_old3dsPhysicsAreas = g_old3dsPhysicsAreas;
            memcpy(record.g_old3dsPhysicsFrames, g_old3dsPhysicsFrames, sizeof(g_old3dsPhysicsFrames));
            record.g_old3dsPhysicsSubsteps = g_old3dsPhysicsSubsteps;
            memcpy(record.g_old3dsRenderPhases, g_old3dsRenderPhases, sizeof(g_old3dsRenderPhases));
            record.g_old3dsSimpleShadowCalls = g_old3dsSimpleShadowCalls;
            record.g_old3dsSimpleShadowDrawn = g_old3dsSimpleShadowDrawn;
            record.g_old3dsUpdateAllCalls = g_old3dsUpdateAllCalls;
            record.g_old3dsUpdateAllMs = g_old3dsUpdateAllMs;
            record.g_old3dsUpdateAllRedundant = g_old3dsUpdateAllRedundant;
            record.g_old3dsUpdateAllRedundantMs = g_old3dsUpdateAllRedundantMs;
            record.g_old3dsUpdateAllRedundantSamples = g_old3dsUpdateAllRedundantSamples;
            record.g_old3dsUpdateAllSamples = g_old3dsUpdateAllSamples;
            record.g_old3dsUpdateAllStaticSkip = g_old3dsUpdateAllStaticSkip;
            record.g_old3dsUpdateAllVerified = g_old3dsUpdateAllVerified;
            record.g_old3dsUpdateAllVerifyMismatch = g_old3dsUpdateAllVerifyMismatch;
            record.g_old3dsTriQueries = g_old3dsTriQueries;
            record.g_old3dsTriHits = g_old3dsTriHits;
            record.g_old3dsTriMisses = g_old3dsTriMisses;
            record.g_old3dsTriEmptyLeaf = g_old3dsTriEmptyLeaf;
            record.g_old3dsTriCandidates = g_old3dsTriCandidates;
            record.g_old3dsTriPlaneTests = g_old3dsTriPlaneTests;
            record.g_old3dsTriWorst = g_old3dsTriWorst;
            record.g_old3dsTriExhausted = g_old3dsTriExhausted;
            record.g_old3dsTriRescanAvoided = g_old3dsTriRescanAvoided;
            record.g_old3dsTriSamples = g_old3dsTriSamples;
            record.g_old3dsTriMs = g_old3dsTriMs;
            memcpy(record.g_old3dsUpdateStages, g_old3dsUpdateStages, sizeof(g_old3dsUpdateStages));
            record.g_picaFrameSubmitMs = g_picaFrameSubmitMs;
            record.g_picaFrameWaitMs = g_picaFrameWaitMs;
            memcpy(record.anim, g_old3dsAnimStats, sizeof(g_old3dsAnimStats));
            memcpy(record.animQueries, g_old3dsAnimQueries, sizeof(g_old3dsAnimQueries));
            memcpy(record.weights, g_old3dsWeights, sizeof(g_old3dsWeights));
            memset(g_old3dsAnimStats, 0, sizeof(g_old3dsAnimStats));
            memset(g_old3dsAnimQueries, 0, sizeof(g_old3dsAnimQueries));
            g_old3dsModelViewUploads = g_old3dsModelViewReuses = 0;
            memset(g_old3dsPhysicsFrames, 0, sizeof(g_old3dsPhysicsFrames));
            g_old3dsPhysicsSubsteps = g_old3dsPhysicsAreas = g_old3dsCollisionPairs = 0;
            g_old3dsFrameBeginOk = g_old3dsFrameBeginBusy = g_old3dsFrameBeginNoDraw = 0;
            g_old3dsFramePresented = 0;
            g_old3dsDrawCalls = g_old3dsDrawVerts = 0;
            g_old3dsDrawDroppedNoFrame = g_old3dsDrawDroppedNotDrawable = 0;
            g_old3dsDrawDroppedNoRoom = 0;
            g_old3dsCmdHighWater = g_old3dsCmdSize = 0;
            memset(g_old3dsRenderPhases, 0, sizeof(g_old3dsRenderPhases));
            memset(g_old3dsEntities, 0, sizeof(g_old3dsEntities));
            memset(g_old3dsOperations, 0, sizeof(g_old3dsOperations));
            memset(g_old3dsBackend, 0, sizeof(g_old3dsBackend));
            memset(g_old3dsPhase2Ticks, 0, sizeof(g_old3dsPhase2Ticks));
            memset(g_old3dsPhase2Draws, 0, sizeof(g_old3dsPhase2Draws));
            memset(g_old3dsPhase2Entities, 0, sizeof(g_old3dsPhase2Entities));
            g_old3dsSimpleShadowCalls = g_old3dsSimpleShadowDrawn = 0;
            g_old3dsLinearFailures = g_old3dsLinearFailureBytes = 0;
            g_old3dsDetectPasses = g_old3dsPairUpdates = 0;
            g_old3dsUpdateAllCalls = g_old3dsUpdateAllRedundant = 0;
            g_old3dsUpdateAllStaticSkip = g_old3dsUpdateAllSamples = 0;
            g_old3dsUpdateAllMs = 0.0;
            g_old3dsUpdateAllRedundantSamples = 0;
            g_old3dsUpdateAllRedundantMs = 0.0;
            g_old3dsUpdateAllVerified = g_old3dsUpdateAllVerifyMismatch = 0;
            g_old3dsTriQueries = g_old3dsTriHits = g_old3dsTriMisses = 0;
            g_old3dsTriEmptyLeaf = g_old3dsTriCandidates = g_old3dsTriPlaneTests = 0;
            g_old3dsTriWorst = g_old3dsTriExhausted = g_old3dsTriRescanAvoided = 0;
            g_old3dsTriSamples = 0;
            g_old3dsTriMs = 0.0;

            memset(g_old3dsUpdateStages, 0, sizeof(g_old3dsUpdateStages));
            updateMs = renderMs = serviceMs = loadMs = 0.0;
            g_picaFrameWaitMs = g_picaFrameSubmitMs = 0.0;
            profileFrames = 0;
            workMs = peakWorkMs = 0.0;
            overBudgetFrames = 0;
            s_old3dsCaptureTicks += svcGetSystemTick() - captureStart;
            ++s_old3dsCaptures;
            lastDiagnosticTick = diagnosticTick;
            lastPresented = presented;
        }
#endif
#ifdef SRR2_OLD_3DS
        g_picaWorldScaleAllowed = (ctx == CONTEXT_GAMEPLAY || ctx == CONTEXT_PAUSE);
#endif
        g_picaMipmapsAllowed =
            ( ctx == CONTEXT_LOADING_GAMEPLAY || ctx == CONTEXT_GAMEPLAY || ctx == CONTEXT_PAUSE ||
              ctx == CONTEXT_LOADING_SUPERSPRINT || ctx == CONTEXT_SUPERSPRINT ||
              ctx == CONTEXT_LOADING_DEMO || ctx == CONTEXT_DEMO ) ? 1 : 0;
        // A 52 ms slice exceeds the 33 ms animation budget. Yield more often
        // during the newspaper entrance, then restore normal loading throughput.
        g_p3dYieldTimeUs = ( ctx == CONTEXT_GAMEPLAY || ctx == CONTEXT_PAUSE ||
                            (ctx == CONTEXT_LOADING_GAMEPLAY && old3dsNewspaperAnimating()) ) ? 8000u : 52000u;
#endif
        DEMOPROFILE( g_DemoProfiler.Start(PROFILE_CHANNEL_ALL); )

        BEGIN_PROFILER_FRAME();

        BEGIN_PROFILE( "GameLoop" )

        unsigned newTime =  radTimeGetMilliseconds();
        unsigned elapsed = newTime - time;
        time = newTime;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        elapsed=NativeBench::Begin(elapsed,ctx);
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
        O3ExperimentExternalCapture(NativeBench::collecting);
#endif
#endif
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
        O3ExperimentBegin(mFrameCount,ctx==CONTEXT_GAMEPLAY);
#endif
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
        if(ctx==CONTEXT_GAMEPLAY && O3ExperimentFixed()) elapsed=50;
#endif

        //
        // Service the windows message loop.
        //
#ifdef RAD_WIN32
        SDL_Event msg;
        while( SDL_PollEvent( &msg ) )
        {
#if SDL_MAJOR_VERSION < 3
            if( msg.type == SDL_QUIT )
#else
            if( msg.type == SDL_EVENT_QUIT )
#endif
            {
                //Chuck someone closed the Window we are going to try to exit the game 
                //if the game isnt in a context that can easily transition to the EXIT context we 
                // are going to call the Launch Dashboard and return to the main loop and shutdown
                if ( GetGameFlow()->GetCurrentContext() != CONTEXT_FRONTEND &&
                     GetGameFlow()->GetCurrentContext() != CONTEXT_GAMEPLAY && 
                     GetGameFlow()->GetCurrentContext() != CONTEXT_PAUSE )
                {
                    GetGame()->GetPlatform()->LaunchDashboard();
                    //return to the winmain and shutdown
                    return;
                }
                //we are in a context that will transition nicely to the Exit context.
                else
                {
                    mpGameFlow->SetContext( CONTEXT_EXIT );
                }               
            }
        }
#endif // RAD_WIN32

        //
        // Service the GameFlow and RenderFlow.
        //

#ifdef SRR2_OLD_3DS
        u64 phaseTick = svcGetSystemTick();
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
        u64 skinUpdateTicks=0, skinRenderTicks=0;
#endif
#endif
        if ( !mpPlatform->PausedForErrors() )
        {
#ifdef SRR2_OLD_3DS
            if(mFrameCount == 0) printf("OLD3DS_RUN flow\n");
#endif
            DEMOPROFILE( g_DemoProfiler.Start(PROFILE_CHANNEL_AI); )
            mpTimerList->Service(); //nv
            { NB_SCOPE(Update); O3_RECUR(Update); mpGameFlow->OnTimerDone(elapsed, NULL); }
#ifdef SRR2_OLD_3DS
            const u64 updateTicks=svcGetSystemTick()-phaseTick;
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
            O3ExperimentPhase(0,updateTicks);
#endif
            updateMs += updateTicks * 1000.0 / SYSCLOCK_ARM11;
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
            skinUpdateTicks=updateTicks;
#endif
            phaseTick = svcGetSystemTick();
#endif
#ifdef SRR2_OLD_3DS
            if(mFrameCount == 0) printf("OLD3DS_RUN render\n");
#endif
            DEMOPROFILE( g_DemoProfiler.Stop(PROFILE_CHANNEL_AI); )

            if( !mExitNow )
            {
                DEMOPROFILE( g_DemoProfiler.Start(PROFILE_CHANNEL_RENDER); )
                {
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
                    O3ExperimentTimer causalRender(O3E_RENDER);
#endif
                    NB_SCOPE(Render); O3_RECUR(Render);
#ifdef RAD_3DS
                    if(Graphics3DS::Stereo())
                    {
                        StereoDepth::Camera profile = StereoDepth::Default;
                        float subjectDistance = 0.0f;
                        if(ctx == CONTEXT_GAMEPLAY || ctx == CONTEXT_PAUSE ||
                           ctx == CONTEXT_DEMO || ctx == CONTEXT_SUPERSPRINT)
                        {
                            SuperCamManager* manager = GetSuperCamManager();
                            SuperCamCentral* central = manager ? manager->GetSCC(0) : NULL;
                            SuperCam* camera = central ? central->GetActiveSuperCam() : NULL;
                            if(camera)
                            {
                                rmt::Vector position, target, distance;
                                camera->GetPosition(&position);
                                camera->GetTarget(&target);
                                distance.Sub(target, position);
                                subjectDistance = distance.Magnitude();
                                switch(camera->GetType())
                                {
                                    case SuperCam::FOLLOW_CAM:
                                    case SuperCam::NEAR_FOLLOW_CAM:
                                    case SuperCam::FAR_FOLLOW_CAM:
                                    case SuperCam::CHASE_CAM:
                                    case SuperCam::REVERSE_CAM:
                                    case SuperCam::WRECKLESS_CAM:
                                    case SuperCam::BURNOUT_CAM:
                                    case SuperCam::SUPER_SPRINT_CAM:
                                        profile = StereoDepth::Driving; break;
                                    case SuperCam::WALKER_CAM:
                                        profile = StereoDepth::Walking; break;
                                    case SuperCam::BUMPER_CAM:
                                    case SuperCam::FIRST_PERSON_CAM:
                                        profile = StereoDepth::Close; break;
                                    case SuperCam::DEFAULT_CAM:
                                        break;
                                    default:
                                        profile = StereoDepth::Cinematic; break;
                                }
                            }
                        }
                        Graphics3DS::UpdateStereoCamera(profile, elapsed, subjectDistance);
                    }
#endif
                    mpRenderFlow->OnTimerDone(elapsed, NULL);
                }
                DEMOPROFILE( g_DemoProfiler.Stop(PROFILE_CHANNEL_RENDER); )
            }
        }
        else if (mpPlatform->IsControllerError()) // if controller unplugged
        {                                         // need to update input manager
            if (InputManager::GetInstance())
            {
                InputManager::GetInstance()->Update( elapsed );
            }
        }
        else
        {
#ifdef RAD_GAMECUBE
            GCManager::GetInstance()->OnTimerDone( elapsed, NULL );
#endif
        }

        //
        // Service FTech subsystems.
        //
#ifdef SRR2_OLD_3DS
        const u64 renderTicks=svcGetSystemTick()-phaseTick;
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
        O3ExperimentPhase(1,renderTicks);
#endif
        renderMs += renderTicks * 1000.0 / SYSCLOCK_ARM11;
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
        skinRenderTicks=renderTicks;
#endif
        phaseTick = svcGetSystemTick();
#endif
        { NB_SCOPE(Service);
        { NB_SCOPE(FileService);
        ::radFileService();
        }
#ifdef SRR2_OLD_3DS
        if(mFrameCount == 0) printf("OLD3DS_RUN files\n");
#endif
        { NB_SCOPE(DebugService);
        ::radDbgComService();
        ::radDebugConsoleService();
        
        if( CommandLineOptions::Get( CLO_MEMORY_MONITOR) )
        {
            ::radMemoryMonitorService();
        }

        //
        // Service the sound renderer.
        //
        }
        { NB_SCOPE(SoundService);
        SoundManager::GetInstance()->Update();
#ifdef SRR2_OLD_3DS
        if(mFrameCount == 0) printf("OLD3DS_RUN sound\n");
#endif

        if ( mpPlatform->PausedForErrors() )
        {
            //
            // [ps] We update sound without a valid context or elapsed time. 
            // We use 0 and NUM_CONTEXTS, since these values are unlikely to
            // happen in the real game.
            //
            SoundManager::GetInstance()->UpdateOncePerFrame( 0, NUM_CONTEXTS, false, true );
        }

        //
        // Spin Pure3D async loading.
        //
        }
        }
        DEMOPROFILE( g_DemoProfiler.Start(PROFILE_CHANNEL_LOAD); )
#ifdef SRR2_OLD_3DS
        const u64 serviceTicks=svcGetSystemTick()-phaseTick;
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
        O3ExperimentPhase(2,serviceTicks);
#endif
        serviceMs += serviceTicks * 1000.0 / SYSCLOCK_ARM11;
        phaseTick = svcGetSystemTick();
#endif
        { NB_SCOPE(Stream); p3d::loadManager->SwitchTask(); }
#ifdef SRR2_OLD_3DS
        const u64 loadTicks=svcGetSystemTick()-phaseTick;
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
        O3ExperimentPhase(3,loadTicks);
#endif
        loadMs += loadTicks * 1000.0 / SYSCLOCK_ARM11;
        ++profileFrames;
#endif
        DEMOPROFILE( g_DemoProfiler.Stop(PROFILE_CHANNEL_LOAD); )

        ++mFrameCount;
#ifdef SRR2_OLD_3DS
        // The original backend can spin while FrameBegin skips a busy GPU.
        // A 30 Hz deadline gives simulation, loading and input one bounded frame
        // budget and lets other threads run on the original ARM11 hardware.
        const u64 frameBudgetTicks = (u64)SYSCLOCK_ARM11 / Graphics3DS::TargetFPS();
        const u64 frameTicks = svcGetSystemTick() - frameStartTick;
        const double currentWorkMs = frameTicks * 1000.0 / SYSCLOCK_ARM11;
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
        O3ExperimentEnd(frameTicks,elapsed);
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
        picaSkinEndFrame(frameTicks,skinUpdateTicks,skinRenderTicks,ctx==CONTEXT_GAMEPLAY);
#endif
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        if(NativeBench::End(frameTicks,ctx)) mExitNow=true;
#endif
        workMs += currentWorkMs;
        if(currentWorkMs > peakWorkMs) peakWorkMs = currentWorkMs;
        if(frameTicks > frameBudgetTicks) ++overBudgetFrames;
        if(Graphics3DS::IsNew() && Graphics3DS::TargetFPS() == 60)
        {
            // Follow the LCD refresh instead of drifting against a nominal 60 Hz clock.
            gspWaitForVBlank();
        }
        else if(frameTicks < frameBudgetTicks)
            svcSleepThread((s64)((frameBudgetTicks - frameTicks) * 1000000000ULL / SYSCLOCK_ARM11));
#endif

        DEMOPROFILE( g_DemoProfiler.Stop(PROFILE_CHANNEL_ALL); )

#ifdef DEMO_MODE_PROFILER
        g_AllowDebugOutput = true;
        if(g_inDemoMode)
        {
            if( !g_DemoProfiler_Started && (g_DemoProfiler_CurrentFrame == g_DemoProfiler_StartFrame) )
            {
                g_DemoProfiler_Started = true;
                g_DemoProfiler.StartRecording();
                rReleasePrintf("Beginning demo profile run\n");
            }

            if(g_DemoProfiler.IsRecording())
            {
                pddiColour colour(255,255,255);
                if( g_DemoProfiler.GetAlertStatus() == DemoProfiler::PROFILER_ALERT_YELLOW)
                    colour.Set(255,255,0);
                else
                if( g_DemoProfiler.GetAlertStatus() == DemoProfiler::PROFILER_ALERT_RED)
                    colour.Set(255,0,0);

                char duff[255];
                sprintf(duff, "%d %d", g_DemoProfiler.GetCurrentFrame(), (g_DemoProfiler.GetSample(0) + 500) / 1000);
                p3d::pddi->DrawString( duff, 10, 400, colour );
            }
            
            g_DemoProfiler.NextFrame();
            g_DemoProfiler_CurrentFrame++;
        }

        g_AllowDebugOutput = false;
#endif // DEMO_MODE_PROFILER

        END_PROFILE( "GameLoop" )

        END_PROFILER_FRAME();
    }
#ifdef SRR2_OLD_3DS
    Old3dsDumpReports("clean-exit");
#endif
#ifdef RAD_3DS
    aptUnhook(&s_aptCookie);
#endif
}


//==============================================================================
// Game::Stop
//==============================================================================
// Synopsis:    Sets the flag to break us out of the game loop.
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
void Game::Stop() 
{
    //
    // Stop any further rendering from happening.
    //
    mExitNow = true;
}


unsigned Game::GetRandomSeed ()
{
    radDate date;
    ::radTimeGetDate (&date);
    unsigned value=( ( date.m_Year << 16 ) | (  date.m_Month << 8 ) | ( date.m_Day ) ) ^ ( ( date.m_Second << 24 ) | ( date.m_Minute << 8 ) | ( date.m_Hour ) );
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    return NativeBench::Seed(value);
#else
    return value;
#endif
}



//******************************************************************************
//
// Private Member Functions
//
//******************************************************************************

//==============================================================================
// Game::Game
//==============================================================================
// Synopsis:    Constructor.
//
// Parameters:  platform - platform on which game should be created
//
// Returns:     N/A.
//
//==============================================================================
Game::Game( Platform* platform ) :
    mpPlatform( platform ),
    mpTimerList( NULL ),
    mpGameFlow( NULL ),
    mpRenderFlow( NULL ),
    mFrameCount( 0 ),
    mExitNow( false ),
    mDemoCount( 0 ),
    mTimeMS( 0 )
{
}


//==============================================================================
// Game::~Game
//==============================================================================
// Synopsis:    Destructor.
//
// Parameters:  None.
//
// Returns:     N/A.
//
//==============================================================================
Game::~Game()
{
}
