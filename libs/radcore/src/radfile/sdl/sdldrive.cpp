//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


//=============================================================================
//
// File:        sdldrive.cpp
//
// Subsystem:   Radical Drive System
//
// Description:	This file contains the implementation of the radSdlDrive class.
//
// Revisions:
//
// Notes:       We keep a serial number when the first file is opened. Then if the
//              media is removed, we don't allow ops until the original serial number
//              is detected, or all files are closed.
//=============================================================================

//=============================================================================
// Include Files
//=============================================================================

#include "pch.hpp"
#ifdef RAD_3DS
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#ifdef RAD_3DS
#include <malloc.h>
#endif
#include <SDL.h>
#include "casepath.hpp"

#if defined(SRR2_OLD3DS_NATIVE_BENCH) || defined(SRR2_OLD3DS_WORLD_RESIDENCY)
#include <worldresidencyassets.hpp>
#endif
#ifdef SRR2_OLD3DS_NATIVE_BENCH
extern "C" {unsigned g_old3dsWorldOrderChoice=0;unsigned g_old3dsWorldResidencyChoice=0;}
static unsigned s_residencyOpens[5][2]={};
static unsigned s_orderOpens[3][2]={};
extern "C" void srr2DumpWorldOrder(FILE* file)
{
    fprintf(file,"STREAM_RESIDENCY selected=%u fixture=world-residency-01 extended_fixture=world-residency-04 reference_terrain=%u reference_sectors=%u resident_terrain=%u resident_sectors=%u expanded_terrain=%u expanded_sectors=%u read_only=1\n",
        __atomic_load_n(&g_old3dsWorldResidencyChoice,__ATOMIC_ACQUIRE),s_residencyOpens[1][0],s_residencyOpens[1][1],s_residencyOpens[2][0],s_residencyOpens[2][1],s_residencyOpens[3][0],s_residencyOpens[3][1]);
    fprintf(file,"STREAM_ASSET_ORDER selected=%u fixture=streaming-order-01 original_z2=%u original_r2=%u ordered_z2=%u ordered_r2=%u read_only=1 baseline_romfs_unchanged=1\n",
        __atomic_load_n(&g_old3dsWorldOrderChoice,__ATOMIC_ACQUIRE),s_orderOpens[1][0],s_orderOpens[1][1],s_orderOpens[2][0],s_orderOpens[2][1]);
    fprintf(file,"STREAM_RESIDENCY_ALL terrain=%u sectors=%u levels=7 read_only=1\n",s_residencyOpens[4][0],s_residencyOpens[4][1]);
}
#endif

enum { SRR2_MAX_TRACKED_FILES = 64 };
struct Srr2TrackedFile
{
    SDL_RWops* rw;
    char       path[ 160 ];
    bool       readOnly;
    bool       closedForSuspend;
};
static Srr2TrackedFile s_Tracked[ SRR2_MAX_TRACKED_FILES ];
static LightLock       s_FileGate = 1;
static bool            s_FilesSuspended = false;

struct Srr2FileGate
{
    Srr2FileGate()  { LightLock_Lock( &s_FileGate ); }
    ~Srr2FileGate() { LightLock_Unlock( &s_FileGate ); }
};

#ifdef SRR2_OLD3DS_WORLD_RESIDENCY
// Admission hashes independent, read-only FILE streams. Holding the tracked-file
// lock here prevents the music streamer from refilling its looping buffer.
struct Srr2FileGateRelease
{
    Srr2FileGateRelease()  { LightLock_Unlock( &s_FileGate ); }
    ~Srr2FileGateRelease() { LightLock_Lock( &s_FileGate ); }
};
#endif

static void srr2TrackOpen( void* handle, const char* path, const char* mode )
{
    for( int i = 0; i < SRR2_MAX_TRACKED_FILES; i++ )
    {
        if( s_Tracked[ i ].rw == NULL )
        {
            s_Tracked[ i ].rw = (SDL_RWops*) handle;
            strncpy( s_Tracked[ i ].path, path, sizeof( s_Tracked[ i ].path ) - 1 );
            s_Tracked[ i ].path[ sizeof( s_Tracked[ i ].path ) - 1 ] = 0;
            s_Tracked[ i ].readOnly = ( strcmp( mode, "rb" ) == 0 );
            s_Tracked[ i ].closedForSuspend = false;
            return;
        }
    }
}

static void srr2TrackClose( void* handle )
{
    for( int i = 0; i < SRR2_MAX_TRACKED_FILES; i++ )
    {
        if( s_Tracked[ i ].rw == (SDL_RWops*) handle )
        {
            s_Tracked[ i ].rw = NULL;
            return;
        }
    }
}

static bool srr2IsRomfs( const char* path )
{
    return strncasecmp( path, "romfs:", 6 ) == 0;
}

extern "C" int srr2SuspendFiles( void )
{
    if( s_FilesSuspended )
    {
        return 0;
    }
    LightLock_Lock( &s_FileGate );
    s_FilesSuspended = true;

    int closed = 0;
    for( int i = 0; i < SRR2_MAX_TRACKED_FILES; i++ )
    {
        Srr2TrackedFile& t = s_Tracked[ i ];
        if( t.rw == NULL || !t.readOnly || srr2IsRomfs( t.path ) ||
            t.rw->type != SDL_RWOPS_STDFILE || t.rw->hidden.stdio.fp == NULL )
        {
            continue;
        }
        fclose( t.rw->hidden.stdio.fp );
        t.rw->hidden.stdio.fp = NULL;
        t.closedForSuspend = true;
        closed++;
    }
    return closed;
}

extern "C" int srr2ResumeFiles( void )
{
    if( !s_FilesSuspended )
    {
        return 0;
    }

    int failed = 0;
    for( int i = 0; i < SRR2_MAX_TRACKED_FILES; i++ )
    {
        Srr2TrackedFile& t = s_Tracked[ i ];
        if( t.rw == NULL || !t.closedForSuspend )
        {
            continue;
        }
        FILE* fp = NULL;
        if( strncasecmp( t.path, "sdmc:", 5 ) != 0 && t.path[ 0 ] != '/' )
        {
            char romfsPath[ 176 ];
            snprintf( romfsPath, sizeof( romfsPath ), "romfs:/%s", t.path );
            fp = fopen( romfsPath, "rb" );
        }
        if( fp == NULL )
        {
            fp = fopen( t.path, "rb" );
        }
        t.rw->hidden.stdio.fp = fp;
        t.closedForSuspend = false;
        if( fp == NULL ) failed++;
    }

    s_FilesSuspended = false;
    LightLock_Unlock( &s_FileGate );
    return failed;
}
#endif
#include <algorithm>
#include <limits.h>
#include "sdldrive.hpp"
#include <string>
#include <SDL.h>
#if SDL_MAJOR_VERSION < 3
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#endif

//=============================================================================
// Public Functions 
//=============================================================================

//=============================================================================
// Function:    radSdlDriveFactory
//=============================================================================
// Description: This member is responsible for constructing a radSdlDriveObject.
//
// Parameters:  pointer to receive drive object
//              pointer to the drive name
//              allocator
//              
// Returns:     
//------------------------------------------------------------------------------

void radSdlDriveFactory
( 
    radDrive**         ppDrive, 
    const char*        pDriveName,
    radMemoryAllocator alloc
)
{
    //
    // Simply constuct the drive object.
    //
    *ppDrive = new( alloc ) radSdlDrive( pDriveName, alloc );
    rAssert( *ppDrive != NULL );
}


//=============================================================================
// Public Member Functions
//=============================================================================

//=============================================================================
// Function:    radSdlDrive::radSdlDrive
//=============================================================================

radSdlDrive::radSdlDrive( const char* pdrivespec, radMemoryAllocator alloc )
    : 
    radDrive( ),
    m_OpenFiles( 0 ),
    m_pMutex( NULL )
{
    m_DrivePath[0] = '\0';
    //
    // Create a mutex for lock/unlock
    //
    radThreadCreateMutex( &m_pMutex, alloc );
    rAssert( m_pMutex != NULL );

    //
    // Create the drive thread.
    //
    m_pDriveThread = new( alloc ) radDriveThread( m_pMutex, alloc );
    rAssert( m_pDriveThread != NULL );

    //
    // Copy the drivename
    //
    radGetDefaultDrive( m_DriveName );
    if ( strcmp(m_DriveName, pdrivespec ) != 0 )
    {
        strncpy( m_DriveName, pdrivespec, radFileDrivenameMax );
        strncpy( m_DrivePath, pdrivespec, radFileFilenameMax );
        m_DriveName[radFileDrivenameMax] = '\0';
        m_DrivePath[radFileFilenameMax] = '\0';
        SDL_strupr( m_DriveName );
        SDL_strlwr( m_DrivePath );
    }

    if(!m_DrivePath[0])
    {
#if SDL_MAJOR_VERSION < 3
#ifdef WIN32
        _getcwd( m_DrivePath, radFileFilenameMax );
        strncat(m_DrivePath, "/", radFileFilenameMax);
#else
        getcwd( m_DrivePath, radFileFilenameMax );
        strncat(m_DrivePath, "/", radFileFilenameMax);
#endif
#else
        char* cwd = SDL_GetCurrentDirectory();
        strncpy(m_DrivePath, cwd, radFileFilenameMax);
        SDL_free(cwd);
#endif
        m_DrivePath[radFileFilenameMax] = '\0';
    }

#if SDL_MAJOR_VERSION < 3
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    printf("NATIVE_FILE_ROOT drive=%s path=%s\n", m_DriveName, m_DrivePath);
#endif
    m_Capabilities = ( radDriveWriteable | radDriveFile );
#else
    m_Capabilities = ( radDriveEnumerable | radDriveWriteable | radDriveDirectory | radDriveFile );
#endif
}

//=============================================================================
// Function:    radSdlDrive::~radSdlDrive
//=============================================================================

radSdlDrive::~radSdlDrive( void )
{
    m_pMutex->Release( );
    m_pDriveThread->Release( );
}

//=============================================================================
// Function:    radSdlDrive::Lock
//=============================================================================
// Description: Start a critical section
//
// Parameters:  
//
// Returns:     
//------------------------------------------------------------------------------

void radSdlDrive::Lock( void )
{
    m_pMutex->Lock( );
}

//=============================================================================
// Function:    radSdlDrive::Unlock
//=============================================================================
// Description: End a critical section
//
// Parameters:  
//
// Returns:     
//------------------------------------------------------------------------------

void radSdlDrive::Unlock( void )
{
    m_pMutex->Unlock( );
}

//=============================================================================
// Function:    radSdlDrive::GetCapabilities
//=============================================================================

unsigned int radSdlDrive::GetCapabilities( void )
{
    return m_Capabilities;
}

//=============================================================================
// Function:    radGcnDVDDrive::GetDriveName
//=============================================================================

const char* radSdlDrive::GetDriveName( void )
{
    return m_DriveName;
}

//=============================================================================
// Function:    radSdlDrive::Initialize
//=============================================================================

radDrive::CompletionStatus radSdlDrive::Initialize( void )
{
    SetMediaInfo();

    //
    // Success
    //
    m_LastError = Success;
    return Complete;
}

//=============================================================================
// Function:    radSdlDrive::OpenFile
//=============================================================================

radDrive::CompletionStatus radSdlDrive::OpenFile
( 
    const char*         fileName, 
    radFileOpenFlags    flags, 
    bool                writeAccess, 
    radFileHandle*      pHandle, 
    unsigned int*       pSize 
)
{
#ifdef RAD_3DS
    Srr2FileGate gate;
#endif
    //
    // Build the full filename
    //
    char fullName[ radFileFilenameMax + 1 ];
    BuildFileSpec( fileName, fullName, radFileFilenameMax + 1 );

    //
    // Translate flags to SDL
    //
    const char* createFlags;
    switch( flags )
    {
    case OpenExisting:
        createFlags = writeAccess ? "rb+" : "rb";
        break;
    case OpenAlways:
        createFlags = "ab+";
        break;
    case CreateAlways:
        createFlags = "wb+";
        break;
    default:
        rAssertMsg( false, "radFileSystem: sdldrive: attempting to open file with unknown flag" );
        return Error;
    }

#if SDL_MAJOR_VERSION < 3
#ifdef RAD_3DS
    *pHandle = NULL;
    // The default drive expands relative names to absolute SD paths. Resolve
    // read-only game assets in the installed title before the SD fallback.
    static const char dataRoot[] = "sdmc:/3ds/simpsons/";
#ifdef SRR2_OLD3DS_WORLD_RESIDENCY
    const char* residentRoot=__atomic_load_n(&g_old3dsResidencyRoot,__ATOMIC_ACQUIRE);
    if(residentRoot && flags==OpenExisting && !writeAccess && strncasecmp(fullName,dataRoot,sizeof(dataRoot)-1)==0){
        const char* relative=fullName+sizeof(dataRoot)-1;
        if(!strncasecmp(relative,"art",3) && (relative[3]=='/' || relative[3]=='\\')){
            const char* name=relative+4;
            for(const char* canonical:WorldResidencyAssets::Files)if(!strcasecmp(name,canonical)){
                const bool admitted = [&] {
                    Srr2FileGateRelease allowAudioReads;
                    return old3dsWorldResidencyAdmit(WorldResidencyAssets::Level(canonical));
                }();
                if(!admitted)break;
                char prepared[radFileFilenameMax+1];const int size=snprintf(prepared,sizeof(prepared),"%s/art/%s",residentRoot,canonical);
                if(size<=0 || unsigned(size)>=sizeof(prepared))return Error;
                *pHandle=SDL_RWFromFile(prepared,createFlags);
                // An admitted level cannot mix in an original sector after an
                // I/O failure beside its already-loaded resident terrain.
                if(!*pHandle){m_LastError=FileNotFound;return Error;}
                strcpy(fullName,prepared);break;
            }
        }
    }
#endif
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    // ResidencyFixtureBegin: exact read-only asset allowlist, no SD-game writes.
    const unsigned residency=__atomic_load_n(&g_old3dsWorldResidencyChoice,__ATOMIC_ACQUIRE);
    if(residency>=1 && residency<=4 && flags==OpenExisting && !writeAccess &&
       strncasecmp(fullName,dataRoot,sizeof(dataRoot)-1)==0){
        const char* relative=fullName+sizeof(dataRoot)-1;
        if(!strncasecmp(relative,"art",3) && (relative[3]=='/' || relative[3]=='\\')){
            const char* name=relative+4;
            static const char* const assets[]={"l2_TERRA.p3d","l2r1.p3d","l2r2.p3d","l2r3.p3d","l2r4.p3d",
                "l2z1.p3d","l2z2.p3d","l2z3.p3d","l2z4.p3d"};
            const auto& allAssets=WorldResidencyAssets::Files;
            const char* const* names=residency==4?allAssets:assets;
            const unsigned assetsCount=residency==4?sizeof(allAssets)/sizeof(allAssets[0]):sizeof(assets)/sizeof(assets[0]);
            for(unsigned i=0;i<assetsCount;++i)if(!strcasecmp(name,names[i])){
                char fixture[radFileFilenameMax+1];
                const char* directory=residency==4 ? "world-residency-05/all" : residency==3 ? "world-residency-04/opaque" :
                    residency==1 ? "world-residency-01/reference" : "world-residency-01/resident";
                const int count=snprintf(fixture,sizeof(fixture),"sdmc:/3ds/hitrun-dev/%s/art/%s",directory,names[i]);
                if(count<=0 || (unsigned)count>=sizeof(fixture))return Error;
                *pHandle=SDL_RWFromFile(fixture,createFlags);
                if(!*pHandle){m_LastError=FileNotFound;return Error;}
                strcpy(fullName,fixture);__atomic_fetch_add(&s_residencyOpens[residency][strstr(names[i],"_TERRA")?0:1],1u,__ATOMIC_RELAXED);
                break;
            }
        }
    }
    // ResidencyFixtureEnd.
    const unsigned order=__atomic_load_n(&g_old3dsWorldOrderChoice,__ATOMIC_ACQUIRE);
    if(order>=1 && order<=2 && flags==OpenExisting && !writeAccess &&
       strncasecmp(fullName,dataRoot,sizeof(dataRoot)-1)==0){
        const char* name=fullName;
        for(const char* p=fullName;*p;++p)if(*p=='/' || *p=='\\')name=p+1;
        const int asset=!strcasecmp(name,"l2z2.p3d")?0:!strcasecmp(name,"l2r2.p3d")?1:-1;
        if(asset>=0){
            char fixture[radFileFilenameMax+1];
            const int count=snprintf(fixture,sizeof(fixture),"sdmc:/3ds/hitrun-dev/streaming-order-01/%s/%s",
                order==1?"original":"ordered",asset==0?"l2z2.p3d":"l2r2.p3d");
            if(count<=0 || (unsigned)count>=sizeof(fixture))return Error;
            *pHandle=SDL_RWFromFile(fixture,createFlags);
            if(!*pHandle){m_LastError=FileNotFound;return Error;}
            strcpy(fullName,fixture);__atomic_fetch_add(&s_orderOpens[order][asset],1u,__ATOMIC_RELAXED);
        }
    }
#endif
    if( !*pHandle && flags == OpenExisting && !writeAccess &&
        strncasecmp( fullName, dataRoot, sizeof( dataRoot ) - 1 ) == 0 )
    {
        char bundled[ radFileFilenameMax + 1 ];
        const int count = snprintf( bundled, sizeof( bundled ), "romfs:/%s",
                                    fullName + sizeof( dataRoot ) - 1 );
        if( count > 0 && (unsigned)count < sizeof( bundled ) )
        {
            *pHandle = SDL_RWFromFile( bundled, createFlags );
            if( !*pHandle )
            {
                char resolved[ radFileFilenameMax + 1 ];
                if( srr2LowercaseDirectories( bundled, resolved, sizeof( resolved ) ) )
                {
                    *pHandle = SDL_RWFromFile( resolved, createFlags );
                    if( *pHandle ) strcpy( bundled, resolved );
                }
                if( !*pHandle && srr2ResolveCasePath( "romfs:/", bundled + 7, resolved, sizeof( resolved ) ) )
                {
                    *pHandle = SDL_RWFromFile( resolved, createFlags );
                    if( *pHandle ) strcpy( bundled, resolved );
                }
            }
            if( *pHandle ) strcpy( fullName, bundled );
        }
    }
    if( !*pHandle )
#endif
    *pHandle = SDL_RWFromFile(fullName, createFlags);
#else
    *pHandle = SDL_IOFromFile( fullName, createFlags );
#endif

    if ( *pHandle )
    {
        m_OpenFiles++;
#ifdef RAD_3DS
        srr2TrackOpen( *pHandle, fullName, createFlags );
#endif
#if SDL_MAJOR_VERSION < 3
        *pSize = SDL_RWsize( (SDL_RWops*)*pHandle );
#else
        *pSize = SDL_GetIOSize( (SDL_IOStream*)*pHandle );
#endif
        m_LastError = Success;
        return Complete;
    }
    else
    {
#ifdef RAD_3DS
        // Report failed opens without adding observers to successful reads or
        // frames. A failed SDL allocation must not look like a missing asset.
        const int failureErrno=errno;
        const struct mallinfo failureHeap=mallinfo();
        printf("FILE_OPEN_FAILURE requested=%s resolved=%s drive=%s base=%s mode=%s errno=%d sdl=%s heap_arena=%u heap_used=%u heap_arena_free=%u linear_free=%lu open_files=%u\n",
            fileName,fullName,m_DriveName,m_DrivePath,createFlags,failureErrno,SDL_GetError(),
            unsigned(failureHeap.arena),unsigned(failureHeap.uordblks),unsigned(failureHeap.fordblks),
            (unsigned long)linearSpaceFree(),unsigned(m_OpenFiles));
#endif
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        const int openError = errno;
        unsigned tracked = 0;
        for(int i = 0; i < SRR2_MAX_TRACKED_FILES; ++i)
            if(s_Tracked[i].rw) ++tracked;
        printf("NATIVE_FILE_OPEN_FAILED requested=%s resolved=%s mode=%s errno=%d open_files=%u tracked=%u sdl=%s\n",
            fileName, fullName, createFlags, openError, unsigned(m_OpenFiles), tracked, SDL_GetError());
#endif
        m_LastError = FileNotFound;
        return Error;
    }
}

//=============================================================================
// Function:    radSdlDrive::CloseFile
//=============================================================================

radDrive::CompletionStatus radSdlDrive::CloseFile( radFileHandle handle, const char* fileName )
{
#ifdef RAD_3DS
    Srr2FileGate gate;
#endif
#if SDL_MAJOR_VERSION < 3
    SDL_RWclose( (SDL_RWops*)handle );
#else
    SDL_CloseIO( (SDL_IOStream*)handle );
#endif
    m_OpenFiles--;
#ifdef RAD_3DS
    srr2TrackClose( handle );
#endif
    return Complete;
}

//=============================================================================
// Function:    radSdlDrive::ReadFile
//=============================================================================

radDrive::CompletionStatus radSdlDrive::ReadFile
( 
    radFileHandle   handle, 
    const char*     fileName,
    IRadFile::BufferedReadState buffState,
    unsigned int    position, 
    void*           pData, 
    unsigned int    bytesToRead, 
    unsigned int*   bytesRead, 
    radMemorySpace  pDataSpace 
)
{
#ifdef RAD_3DS
    Srr2FileGate gate;
#endif
    rAssertMsg( pDataSpace == radMemorySpace_Local, 
                "radFileSystem: radSdlDrive: External memory not supported for reads." );

    //
    // set file pointer
    //
#if SDL_MAJOR_VERSION < 3
    if ( SDL_RWseek( (SDL_RWops*)handle, position, RW_SEEK_SET ) >= 0 )
    {
        if (SDL_RWread( (SDL_RWops*)handle, pData, 1, bytesToRead ) > 0 )
        {
#else
    if ( SDL_SeekIO( (SDL_IOStream*)handle, position, SDL_IO_SEEK_SET ) >= 0 )
    {
        if ( SDL_ReadIO( (SDL_IOStream*)handle, pData, bytesToRead ) > 0 )
        {
#endif
            //
            // Successful read!
            //
            
            //
            // Change this during buffered read!!
            //
            *bytesRead = bytesToRead;
            m_LastError = Success;
            return Complete;
        }
    }

    //
    // Failed!
    //
    m_LastError = FileNotFound;
    return Error;
}

//=============================================================================
// Function:    radSdlDrive::WriteFile
//=============================================================================

radDrive::CompletionStatus radSdlDrive::WriteFile
( 
    radFileHandle     handle,
    const char*       fileName,
    IRadFile::BufferedReadState buffState,
    unsigned int      position, 
    const void*       pData, 
    unsigned int      bytesToWrite, 
    unsigned int*     bytesWritten, 
    unsigned int*     pSize, 
    radMemorySpace    pDataSpace 
)
{
#ifdef RAD_3DS
    Srr2FileGate gate;
#endif
    if ( !( m_Capabilities & radDriveWriteable ) )
    {
        rWarningMsg( m_Capabilities & radDriveWriteable, "This drive does not support the WriteFile function." );
        return Error;
    }

    rAssertMsg( pDataSpace == radMemorySpace_Local, 
                "radFileSystem: radSdlDrive: External memory not supported for reads." );

    //
    // do the write
    //
#if SDL_MAJOR_VERSION < 3
    if ( SDL_RWseek( (SDL_RWops*)handle, position, RW_SEEK_SET ) >= 0 )
    {
        *bytesWritten = SDL_RWwrite( (SDL_RWops*)handle, pData, 1, bytesToWrite );
#else
    if ( SDL_SeekIO( (SDL_IOStream*)handle, position, SDL_IO_SEEK_SET ) >= 0 )
    {
        *bytesWritten = SDL_WriteIO( (SDL_IOStream*)handle, pData, bytesToWrite );
#endif
        if ( *bytesWritten == bytesToWrite )
        {
            //
            // Sucessful write
            //
#if SDL_MAJOR_VERSION < 3
            *pSize = SDL_RWsize( (SDL_RWops*)handle );
#else
            *pSize = SDL_GetIOSize( (SDL_IOStream*)handle );
#endif
            m_LastError = Success;
            return Complete;
        }
    }

    //
    // Failed!
    //
    m_LastError = FileNotFound;
    return Error;
}

#if SDL_MAJOR_VERSION > 2
//=============================================================================
// Function:    radSdlDrive::FindFirst
//=============================================================================

radDrive::CompletionStatus radSdlDrive::FindFirst
( 
    const char*                 searchSpec, 
    IRadDrive::DirectoryInfo*   pDirectoryInfo, 
    radFileDirHandle*           pHandle,
    bool                        firstSearch
)
{
    //
    // Find first
    //
    const char* pattern = strrchr(searchSpec, '\\');
    std::string path;
    if (!pattern)
        pattern = strrchr(searchSpec, '/');
    if (pattern)
        path = std::string(searchSpec, pattern - searchSpec);
    else
        pattern = searchSpec;
    path = m_DrivePath + path;
    std::replace(path.begin(), path.end(), '\\', '/');

    char** handle = SDL_GlobDirectory( path.c_str(), pattern, SDL_GLOB_CASEINSENSITIVE, NULL );
    if ( handle )
    {
        SDL_PathInfo info;
        if ( SDL_GetPathInfo( handle[0], &info))
            m_LastError = TranslateDirInfo( pDirectoryInfo, &info, pHandle );
        else
            m_LastError = TranslateDirInfo( pDirectoryInfo, NULL, pHandle );
        // HACK: We don't need the first element anymore, so use it to store the iterator
        ((char***)handle)[0] = handle;
    }
    else
    {
        m_LastError = FileNotFound;
    }

    //
    // Fill in our directory info structure
    //
    if ( m_LastError == Success )
    {
        return Complete;
    }
    else
    {
        return Error;
    }
}

//=============================================================================
// Function:    radSdlDrive::FindNext
//=============================================================================

radDrive::CompletionStatus radSdlDrive::FindNext( radFileDirHandle* pHandle, IRadDrive::DirectoryInfo* pDirectoryInfo )
{
    //
    // If we don't have a handle, return file not found.
    //
    if ( *pHandle == NULL )
    {
        m_LastError = FileNotFound;
        return Error;
    }

    //
    // Find the next entry
    //
    char*** handle = (char***)*pHandle;
    *handle++;
    SDL_PathInfo info;
    if ( SDL_GetPathInfo(  **handle, &info ))
        m_LastError = TranslateDirInfo( pDirectoryInfo, &info, pHandle );
    else
        m_LastError = TranslateDirInfo( pDirectoryInfo, NULL, pHandle );
    
    if ( m_LastError == Success )
    {
        m_LastError = Success;
        return Complete;
    }
    else
    {
        m_LastError = FileNotFound;
        return Error;
    }
}

//=============================================================================
// Function:    radSdlDrive::FindClose
//=============================================================================

radDrive::CompletionStatus radSdlDrive::FindClose( radFileDirHandle* pHandle )
{
    SDL_free( *pHandle );
    *pHandle = NULL;

    return Complete;
}

//=============================================================================
// Function:    radSdlDrive::CreateDir
//=============================================================================

radDrive::CompletionStatus radSdlDrive::CreateDir( const char* pName )
{
    rWarningMsg( m_Capabilities & radDriveDirectory, 
        "This drive does not support the CreateDir function." );

    //
    // Build the full filename
    //
    char fullSpec[ radFileFilenameMax + 1 ];
    BuildFileSpec( pName, fullSpec, radFileFilenameMax + 1 );

    if ( SDL_CreateDirectory( fullSpec ) )
    {
        m_LastError = Success;
        return Complete;
    }
    else
    {
        m_LastError = FileNotFound;
        return Error;
    }
}

//=============================================================================
// Function:    radSdlDrive::DestroyDir
//=============================================================================

radDrive::CompletionStatus radSdlDrive::DestroyDir( const char* pName )
{
    rWarningMsg( m_Capabilities & radDriveDirectory,
        "This drive does not support the DestroyDir function." );

    //
    // Someday check if pName is a dir!
    //

    //
    // Build the full filename
    //
    char fullSpec[ radFileFilenameMax + 1 ];
    BuildFileSpec( pName, fullSpec, radFileFilenameMax + 1 );

    if ( SDL_RemovePath( fullSpec ) )
    {
        m_LastError = Success;
        return Complete;
    }
    else
    {
        m_LastError = FileNotFound;
        return Error;
    }
}

//=============================================================================
// Function:    radSdlDrive::DestroyFile
//=============================================================================

radDrive::CompletionStatus radSdlDrive::DestroyFile( const char* filename )
{
    rWarningMsg( m_Capabilities & radDriveWriteable, "This drive does not support the DestroyFile function." );

    //
    // Someday check if the file is open!
    //

    //
    // Build the full filename
    //
    char fullSpec[ radFileFilenameMax + 1 ];
    BuildFileSpec( filename, fullSpec, radFileFilenameMax + 1 );

    if ( SDL_RemovePath( fullSpec ) )
    {
        m_LastError = Success;
        return Complete;
    }
    else
    {
        m_LastError = FileNotFound;
        return Error;
    }
}
#endif

//=============================================================================
// Private Member Functions
//=============================================================================

//=============================================================================
// Function:    radSdlDrive::SetMediaInfo
//=============================================================================

void radSdlDrive::SetMediaInfo( void )
{
    //
    // Get volume information.
    //
    const char* realDriveName = m_DriveName;

    //rAssert( strlen( realDriveName ) == 2 );
    strcpy(m_MediaInfo.m_VolumeName, realDriveName );
    //strcat(m_MediaInfo.m_VolumeName, "\\");

    m_MediaInfo.m_SectorSize = SDL_DEFAULT_SECTOR_SIZE;

    /*
    if(!error)
    {
        m_MediaInfo.m_MediaState = IRadDrive::MediaInfo::MediaPresent;
        m_MediaInfo.m_FreeSpace = space.free;

        //
        // No file limit, so set it to the available space
        //
        m_MediaInfo.m_FreeFiles = space.available / m_MediaInfo.m_SectorSize;
        m_LastError = Success;
    }
    else
    */
    {
        //
        // Don't have media info, so fill structure in with dummy info
        //
        m_MediaInfo.m_MediaState = IRadDrive::MediaInfo::MediaPresent;
        m_MediaInfo.m_FreeSpace = UINT_MAX;
        m_MediaInfo.m_FreeFiles = m_MediaInfo.m_FreeSpace / m_MediaInfo.m_SectorSize;
        m_LastError = Success;
    }
}

//=============================================================================
// Function:    radSdlDrive::BuildFileSpec
//=============================================================================

void radSdlDrive::BuildFileSpec( const char* fileName, char* fullName, unsigned int size )
{
    std::string path(m_DrivePath);
    path += fileName;
    std::replace(path.begin(), path.end(), '\\', '/');

    strncpy( fullName, path.c_str(), size - 1 );
    fullName[ size - 1 ] = '\0';
}

#if SDL_MAJOR_VERSION > 2
//=============================================================================
// Function:    radSdlDrive::TranslateDirInfo
//=============================================================================
// Description: Translate the directory info and return an error status. A handle
//              with value directory_iterator() means the find_first/next call
//              failed and needs to be checked if something went wrong or if the
//              search just ended.
//
// Parameters:  
//              
// Returns:     
//------------------------------------------------------------------------------

radFileError radSdlDrive::TranslateDirInfo
( 
    IRadDrive::DirectoryInfo*   pDirectoryInfo, 
    const SDL_PathInfo*         pPathInfo,
    const radFileDirHandle*     pHandle
)
{
    char*** handle = (char***)*pHandle;
    if ( !pPathInfo || !*handle )
    {
        //
        // Either we failed or we're out of games.
        //
        if ( !pPathInfo )
        {
            return FileNotFound;
        }
        else
        {
            pDirectoryInfo->m_Name[0] = '\0';
            pDirectoryInfo->m_Type = IRadDrive::DirectoryInfo::IsDone;
        }
    }
    else
    {
        strncpy( pDirectoryInfo->m_Name, **handle, radFileFilenameMax );
        pDirectoryInfo->m_Name[ radFileFilenameMax ] = '\0';

        if ( pPathInfo->type == SDL_PATHTYPE_DIRECTORY )
        {
            pDirectoryInfo->m_Type = IRadDrive::DirectoryInfo::IsDirectory;
        }
        else
        {
            pDirectoryInfo->m_Type = IRadDrive::DirectoryInfo::IsFile;
        }
    }
    return Success;
}
#endif
