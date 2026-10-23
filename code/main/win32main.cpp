#include <main/graphics3ds.h>
#ifdef SRR2_OLD3DS_PARTICLE_CLIP
extern "C" void old3dsDumpParticleClip();
#endif
#ifdef SRR2_OLD3DS_WHEEL_LOD
extern "C" void old3dsDumpWheelLOD();
extern "C" void picaDumpWheelLodMemory();
#endif
#ifdef SRR2_OLD3DS_JOINT_AXIS_ONCE
extern "C" void old3dsDumpJointAxisReuse();
#endif
#ifdef SRR2_OLD3DS_BONE_USAGE
extern "C" void old3dsDumpBoneUsage();
#endif
#ifdef SRR2_OLD3DS_COLL_TRANSFORM_ONCE
extern "C" void old3dsDumpCollisionTransforms();
#endif
#ifdef SRR2_OLD3DS_MATERIAL_WRITES
extern "C" void picaDumpMaterialWrites();
#endif
#ifdef SRR2_OLD3DS_INERTIA_TENSOR_CACHE
extern "C" void old3dsDumpInertiaTensors();
#endif
#ifdef SRR2_OLD3DS_JOINT_VECTOR_REUSE
extern "C" void old3dsDumpJointVectors();
#endif
#if defined(SRR2_OLD_3DS) && defined(SRR2_OLD3DS_COLL_PREP)
extern "C" void old3dsDumpPairTail();
#endif
#ifdef SRR2_OLD3DS_SPHERE_WALK_CACHE
extern "C" void old3dsDumpSphereWalkCache();
#endif
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
extern "C" void old3dsValidateJointRotationReuse();
#endif
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
extern "C" void picaDumpVramTextures();
#endif
#ifdef SRR2_OLD3DS_VEHICLE_PROFILE
extern "C" void old3dsDumpVehicleProfile();
#endif
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
extern "C" void picaDumpClipBoundsStats();
#endif
#ifdef SRR2_OLD3DS_PGO_GENERATE
extern "C" void old3dsDumpProfile();
#endif
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
extern "C" void picaDumpUnlitProjectionStats();
#endif
//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        xboxmain.cpp
//
// Description: This file contains the main enrty point to the game.
//
// History:     + Based on xbox main and winmain from squidney
//
//=============================================================================

//========================================
// System Includes
//========================================
// Standard Library
#include <string.h>
// Foundation Tech
#include <raddebug.hpp>
#include <radobject.hpp>
// file reading before radtech
#include <stdio.h>
#include <SDL_main.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

#ifdef RAD_VITA
#include <unistd.h>
#include <psp2/kernel/clib.h>
unsigned int sceLibcHeapSize = 16 * 1024 * 1024;
#endif

#ifdef RAD_3DS
#include <unistd.h>
#include <3ds.h>
#include <citro3d.h>
#include <sys/stat.h>

extern "C" void radSoundHal3dsShutdownForExit( void );
#ifdef SRR2_OLD_3DS
extern "C" void picaRelease3DIndicator(void);
#endif

#define SRR2_3DS_DATA_PATH "sdmc:/3ds/simpsons"

extern "C" {
unsigned int __stacksize__
__attribute__((used, retain, section(".data.__stacksize__")))
= 2 * 1024 * 1024;
u32 __ctru_linear_heap_size = 28 * 1024 * 1024;
#ifdef SRR2_OLD_3DS
// Bound emulation to a 64 MiB application budget, including executable and
// linear memory. Do not let an emulator's larger homebrew region hide overflow.
u32 __ctru_heap_size = 30 * 1024 * 1024;
#endif
}
#endif

//========================================
// Project Includes
//========================================
#include <main/game.h>
#include <main/nativebench.h>
#include <main/win32platform.h>
#include <main/singletons.h>
#include <main/commandlineoptions.h>
#include <memory/memoryutilities.h>
#include <memory/srrmemory.h>
#include <p3d/entity.hpp>

//========================================
// Forward Declarations
//========================================
static void ProcessCommandLineArguments( int argc, char *argv[] );

static void ProcessCommandLineArgumentsFromFile();

static void LogOutputFunction( void *userdata, int category, SDL_LogPriority priority, const char *message )
{
#ifdef RAD_VITA
    sceClibPrintf( "%s\n", message );
#else
    printf( "%s\n", message );
    fflush( stdout );
#endif
}

//=============================================================================
// Function:    WinMain
//=============================================================================
//
// Description: Main Windows entry point.
// 
// Parameters:  win32 parameters
//
// Returns:     win32 return.
//
//=============================================================================
extern "C" int main( int argc, char *argv[] )
{
#ifdef __SWITCH__
#ifdef RAD_DEBUG
    socketInitializeDefault();
    nxlinkStdio();
#endif
    romfsInit();
#endif
#ifdef RAD_VITA
	chdir( "ux0:data/simpsons" );
#endif
#ifdef RAD_3DS
    bool isNew3DS = false;
    const bool new3dsSpeedup = R_SUCCEEDED( APT_CheckNew3DS( &isNew3DS ) ) && isNew3DS;
    // Keep original-model compatibility while enabling New 3DS CPU and L2 acceleration.
    osSetSpeedupEnable( new3dsSpeedup );
    u8 systemModel = 255;
    if(R_SUCCEEDED(cfguInit()))
    {
        if(R_FAILED(CFGU_GetSystemModel(&systemModel))) systemModel = 255;
        cfguExit();
    }
    Graphics3DS::Initialize(systemModel, argc, argv);
    gfxInitDefault();
    romfsInit();
    // Installed titles keep bundled assets in RomFS and writable saves on SD.
    mkdir( "sdmc:/3ds", 0777 );
    mkdir( SRR2_3DS_DATA_PATH, 0777 );
    chdir( SRR2_3DS_DATA_PATH );
#ifdef SRR2_OLD_3DS
    freopen("old3ds.log", "w", stdout);
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("OLD3DS_BOOT model=%s loader_region=%lu heap_limit=%lu linear_limit=%lu linear_free=%lu\n",
        isNew3DS ? "new" : "old", (unsigned long)osGetMemRegionSize(MEMREGION_APPLICATION),
        (unsigned long)__ctru_heap_size, (unsigned long)__ctru_linear_heap_size,
        (unsigned long)linearSpaceFree());
    printf("OLD3DS_CPU new3ds_speedup=%u\n", new3dsSpeedup ? 1u : 0u);
    // Tie a log to the executable that produced it. The earlier hardware log
    // carried no build identifier, so it could not be matched to a binary.
    printf("OLD3DS_BUILD id=%s date=%s time=%s\n",
        SRR2_OLD3DS_BUILD_ID, __DATE__, __TIME__);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    // The small netloader artifact contains frontend assets, while gameplay
    // requires a full bundle or loose retail data. Fail before entering the
    // engine's modal file-error loop so an invalid benchmark returns normally.
    FILE* benchmarkAssets=fopen("romfs:/scripts.rcf","rb");
    if(!benchmarkAssets)benchmarkAssets=fopen("scripts.rcf","rb");
    if(!benchmarkAssets)
    {
        printf("OLD3DS_BENCH_ASSETS missing=scripts.rcf gameplay_frames=0 returning_to_launcher=1\n");
        romfsExit();gfxExit();return 1;
    }
    fclose(benchmarkAssets);
#endif
#endif
#endif

    //
    // Pick out and store command line settings.
    //
    CommandLineOptions::InitDefaults();
    ProcessCommandLineArguments( argc, argv );
    ProcessCommandLineArgumentsFromFile();
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    NativeBench::Init(argc,argv);
#endif
#ifdef SRR2_OLD_3DS
    printf("OLD3DS_INIT commandline_complete\n");
#endif

    //
    // Initialize SDL subsystems
    //
#if defined RAD_3DS
    SDL_Init( SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER );
#ifdef SRR2_OLD_3DS
    printf("OLD3DS_INIT sdl_complete error=%s\n", SDL_GetError());
#endif

    SDL_LogSetOutputFunction( LogOutputFunction, NULL );
#ifdef SRR2_OLD_3DS
    extern bool g_AllowDebugOutput;
    g_AllowDebugOutput = true;
#endif
#elif SDL_MAJOR_VERSION < 3
    SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER );

    SDL_LogSetOutputFunction( LogOutputFunction, NULL );
#else
    SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_GAMEPAD );

    SDL_SetLogOutputFunction( LogOutputFunction, NULL );
#endif

    //
    // Have to get FTech setup first so that we can use all the memory services.
    // The initialize window call will fail if another Simpsons window exists. In
    // this case, we exit.
    //
    if( !Win32Platform::InitializeWindow() )
    {
        return 0;
    }
    Win32Platform::InitializeFoundation();
#ifdef SRR2_OLD_3DS
    printf("OLD3DS_INIT foundation_complete\n");
#endif

    srand (Game::GetRandomSeed ());


    // Now disable the default heap
    //
#ifndef RAD_RELEASE
    tName::SetAllocator (GMA_DEBUG);

    //g_HeapActivityTracker.EnableHeapAllocs (GMA_DEFAULT, false);
    //g_HeapActivityTracker.EnableHeapFrees (GMA_DEFAULT, false);
#endif

    HeapMgr()->PushHeap (GMA_PERSISTENT);

    //
    // Instantiate all the singletons before doing anything else.
    //
    CreateSingletons();
#ifdef SRR2_OLD_3DS
    printf("OLD3DS_INIT singletons_complete\n");
#endif

    //
    // Construct the platform object.
    //
    Win32Platform* pPlatform = Win32Platform::CreateInstance();
    rAssert( pPlatform != NULL );

    //
    // Create the game object.
    //
    Game* pGame = Game::CreateInstance( pPlatform );
    rAssert( pGame != NULL );


    //
    // Initialize the game.
    //
    pGame->Initialize();
#ifdef SRR2_OLD_3DS
    printf("OLD3DS_INIT game_complete\n");
#endif

    HeapMgr()->PopHeap (GMA_PERSISTENT);

    //
    // Run it!  Control will not return from here until the game is stopped.
    //
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    old3dsValidateJointRotationReuse();
#endif
    pGame->Run();
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    NativeBench::Dump();
#endif
#ifdef SRR2_OLD_3DS
    picaRelease3DIndicator();
#endif
#if defined(SRR2_OLD_3DS) && defined(SRR2_OLD3DS_COLL_PREP)
    old3dsDumpPairTail();
#endif
#ifdef SRR2_OLD3DS_COLL_TRANSFORM_ONCE
    old3dsDumpCollisionTransforms();
#endif
#ifdef SRR2_OLD3DS_PARTICLE_CLIP
    old3dsDumpParticleClip();
#endif
#ifdef SRR2_OLD3DS_MATERIAL_WRITES
    picaDumpMaterialWrites();
#endif
#ifdef SRR2_OLD3DS_INERTIA_TENSOR_CACHE
    old3dsDumpInertiaTensors();
#endif
#ifdef SRR2_OLD3DS_JOINT_VECTOR_REUSE
    old3dsDumpJointVectors();
#endif
#ifdef SRR2_OLD3DS_WHEEL_LOD
    old3dsDumpWheelLOD();
    picaDumpWheelLodMemory();
#endif
#ifdef SRR2_OLD3DS_JOINT_AXIS_ONCE
    old3dsDumpJointAxisReuse();
#endif
#ifdef SRR2_OLD3DS_BONE_USAGE
    old3dsDumpBoneUsage();
#endif
#ifdef SRR2_OLD3DS_SPHERE_WALK_CACHE
    old3dsDumpSphereWalkCache();
#endif
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    picaDumpVramTextures();
#endif
#ifdef SRR2_OLD3DS_VEHICLE_PROFILE
    old3dsDumpVehicleProfile();
#endif
#ifdef SRR2_OLD3DS_CLIP_BOUNDS
    picaDumpClipBoundsStats();
#endif
#ifdef SRR2_OLD3DS_PGO_GENERATE
    old3dsDumpProfile();
#endif
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    picaDumpUnlitProjectionStats();
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
    extern void picaSkinDump();
    picaSkinDump();
#endif


#ifdef RAD_3DS
    radSoundHal3dsShutdownForExit( );
    C3D_Fini( );
    gfxExit( );
    romfsExit( );
    _exit( 0 );
#endif

    //
    // Terminate the game (this frees all resources allocated by the game).
    //
    pGame->Terminate();

    //
    // Dump all the singletons.
    //
    DestroySingletons();

    //
    // Destroy the game object.
    //
    Game::DestroyInstance();

    //
    // Shutdown the platform.
    //
    pPlatform->ShutdownPlatform();

    //
    // Destroy the game and platform (do it in this order in case the game's 
    // destructor references the platform.
    //
    Win32Platform::DestroyInstance();

    // As a last thing, shut down the memory.
    Win32Platform::ShutdownMemory();

    // Re-enable the default heap
    //
#ifndef RAD_RELEASE
    tName::SetAllocator (RADMEMORY_ALLOC_DEFAULT);
#endif

    //
    // Shutdown SDL subsystems
    //
    SDL_Quit();

    //
    // Pass any error codes back to the operating system.
    //
    return 0;
}


//=============================================================================
// Function:    ProcessCommandLineArguments
//=============================================================================
//
// Description: Pick out the command line options and store them.
// 
// Parameters:  None.
//
// Returns:     None.
//
//=============================================================================
void ProcessCommandLineArguments(int argc, char* argv[])
{
    rDebugPrintf( "*************************************************************************\n" );
    rDebugPrintf( "Command Line Args:\n" );

    //
    // Pick out all the command line options and store them in GameDB.
    // Also dump them to the output for handy dandy viewing.
    //
    for (int i = 1; i < argc; i++ )
    {
        rDebugPrintf( "arg%d: %s\n", i, argv[i] );

        CommandLineOptions::HandleOption( argv[i] );
    }

    if( !CommandLineOptions::Get( CLO_ART_STATS ) )
    {
        //CommandLineOptions::HandleOption( "noheaps" );
    }

    rDebugPrintf( "*************************************************************************\n" );
}



void ProcessCommandLineArgumentsFromFile()
{
#ifndef FINAL

    //Chuck: looking for additional command line args being passed in from a file
    //its for QA testing etc.

    FILE* pfile = fopen( "command.txt", "r" );

    if (pfile != NULL)
    {
        int ret = fseek( pfile, 0, SEEK_END ); 
        rAssert( ret == 0 );

        int len = ftell( pfile );
        rAssertMsg( len < 256, "Command line file too large to process." );

        rewind( pfile );

        if( len > 0 && len < 256 )
        {
            char commandlinestring[256] = {0};

            fgets( commandlinestring, 256, pfile );
                    
            char* argument = strtok(commandlinestring," ");
            while (argument != NULL)
            {
                CommandLineOptions::HandleOption(argument);
                argument=strtok(NULL," ");
            }
        }

        fclose( pfile );
    }
#endif //FINAL
} //end of Function
