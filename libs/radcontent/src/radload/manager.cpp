#include <main/old3dspools.h>
//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

#include <radload/manager.hpp>
#include <main/nativebench.h>
#include <string.h>
#include <radload/utility/hashtable.hpp>
#include <radload/utility/queue.hpp>
#include <radfile.hpp>

#ifdef RADLOAD_USE_WATCHER
#include <raddebugwatch.hpp>
#endif

#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <radload/loadtiming.hpp>
#include <radnativeseek.hpp>
#include <cstdio>
namespace {
radLoadTiming::State workerTiming;
bool WorkerTimingEnabled(){return NativeBench::serviceProfile && NativeBench::collecting && !NativeBench::OnCaptureThread();}
struct WorkerRegion {
    bool active=false;const char* previous=nullptr;char ownedLabel[96];
    WorkerRegion(const WorkerRegion&)=delete;WorkerRegion& operator=(const WorkerRegion&)=delete;
    explicit WorkerRegion(const char* label){
        if(WorkerTimingEnabled()){
            std::strncpy(ownedLabel,label?label:"unnamed",sizeof(ownedLabel)-1);
            ownedLabel[sizeof(ownedLabel)-1]=0;
            previous=workerTiming.Enter(ownedLabel,svcGetSystemTick(),NativeBench::CurrentRouteMs());active=true;
        }
    }
    ~WorkerRegion(){if(active){
        if(WorkerTimingEnabled())workerTiming.Leave(previous,svcGetSystemTick(),NativeBench::CurrentRouteMs());
        else workerTiming.Cancel(previous);
    }}
};
void PauseWorkerTiming(){if(WorkerTimingEnabled())workerTiming.Pause(svcGetSystemTick(),NativeBench::CurrentRouteMs());}
void ResumeWorkerTiming(){if(WorkerTimingEnabled())workerTiming.Resume(svcGetSystemTick());}
}
radLoadTiming::StageRegion::StageRegion(unsigned id){
    if(WorkerTimingEnabled()){
        previous=workerTiming.EnterStage(id,svcGetSystemTick(),NativeBench::CurrentRouteMs());active=true;
    }
}
radLoadTiming::StageRegion::~StageRegion(){if(active){
    if(WorkerTimingEnabled())workerTiming.LeaveStage(previous,svcGetSystemTick(),NativeBench::CurrentRouteMs());
    else workerTiming.CancelStage(previous);
}}
extern "C" void radLoadDumpWorkerSlices(FILE* file){
    fprintf(file,"NATIVE_LOAD_WORKER enabled=%u records=%u samples=%u main_mutex_serialized=1 excludes_yielded_time=1 wall_not_cpu=1 diagnostic_only=1\n",NativeBench::serviceProfile,workerTiming.count,workerTiming.samples);
    for(unsigned i=0;i<workerTiming.count;++i){const auto& s=workerTiming.longest[i];
        const char* phase="chunk";
        switch(s.stage){
            case radLoadTiming::NativeTexture:phase="native_texture";break;
            case radLoadTiming::NativeTextureHash:phase="native_hash";break;
            case radLoadTiming::NativeTextureOpen:phase="native_open";break;
            case radLoadTiming::NativeTextureSeek:phase="native_seek";break;
            case radLoadTiming::NativeTextureRead:phase="native_read_verify";break;
            case radLoadTiming::TextureAllocate:phase="texture_allocate";break;
            case radLoadTiming::TextureDecode:phase="texture_decode";break;
            case radLoadTiming::TextureFlush:phase="texture_flush";break;
            case radLoadTiming::SkinInput:phase="skin_input";break;
            case radLoadTiming::SkinPrepare:phase="skin_prepare";break;
        }
        fprintf(file,"NATIVE_LOAD_SLICE file=%s chunk=0x%08x phase=%s route_ms=%u start_tick=%llu span_us=%.3f\n",s.label,s.stage,phase,s.routeMs,(unsigned long long)s.start,s.ticks*1000000.0/SYSCLOCK_ARM11);
    }
}
#endif

#ifdef SRR2_OLD3DS_NATIVE_BENCH
namespace radNativeSeek {
bool enabled=false;
bool stageInput=false;
static bool draining=false;
static void* owner=nullptr;
static unsigned begun=0,completed=0;
static u64 started=0,totalOutside=0,maxOutside=0;
}
#endif

radLoadManagerWrapper radLoad;

ILoadManager* ILoadManager::s_instance = NULL;

void radLoadInitialize( radLoadInit* init )
{
    if( !init )
    {
        radMemoryAllocator old = ::radMemorySetCurrentAllocator( RADMEMORY_ALLOC_TEMP );
        init = new radLoadInit();
        ::radMemorySetCurrentAllocator( old );
    }
    ILoadManager::s_instance = new radLoadManager( *init );
    delete init;
}

void radLoadTerminate()
{
    radLoad->Terminate();
    ILoadManager::s_instance = NULL;
}

void radLoadService()
{
    radLoad->Service();
}

ILoadManager* radLoadInstance()
{
    return ILoadManager::s_instance;
}

#ifdef SRR2_OLD3DS_NATIVE_BENCH
namespace radNativeSeek {
void* Begin()
{
    if(!enabled || draining || !NativeBench::collecting)return nullptr;
    auto* manager=static_cast<radLoadManager*>(radLoadInstance());
    return manager && manager->BeginNativeSeek()?manager:nullptr;
}
void End(void* selected)
{
    static_cast<radLoadManager*>(selected)->EndNativeSeek();
}
void Drain()
{
    if(!owner || !NativeBench::OnCaptureThread())return;
    // Restore the original paused-worker assumption before benchmark teardown.
    const bool previous=draining;draining=true;
    while(owner)static_cast<radLoadManager*>(owner)->SwitchTasks();
    draining=previous;
}
void Dump(FILE* file)
{
    fprintf(file,"NATIVE_SEEK_LEND enabled=%u begun=%u completed=%u active=%u total_outside_us=%.3f max_outside_us=%.3f outside_wall_not_cpu=1 scope=whole_session\n",enabled,begun,completed,owner!=nullptr,totalOutside*1000000.0/SYSCLOCK_ARM11,maxOutside*1000000.0/SYSCLOCK_ARM11);
}
}
#endif

radLoadManager::radLoadManager( radLoadInit& init )
:
m_bSyncLoading( false ),
m_bDone( false ),
#ifdef RADLOAD_GATHER_STATS
m_totalLoads( 0 ),
m_completedLoads( 0 ),
m_pendingLoads( 0 ),
m_maxPendingLoads( 0 ),
m_minLoadTime( 0xFFFFFFFF ),
m_maxLoadTime( 0 ),
m_avgLoadTime( 0 ),
m_minQueuedTime( 0xFFFFFFFF ),
m_maxQueuedTime( 0 ),
m_avgQueuedTime( 0 ),
#endif
m_pCurrent( NULL ),
m_pFileLoaders( NULL ),
m_pDataLoaders( NULL ),
m_pLoadQueue( NULL ),
m_pThread( NULL ),
m_pMutex( NULL )
{
    // Make our hash tables so that they will never resize or repack
    m_pFileLoaders = new RefHashTable<radLoadFileLoader>( init.fileLoaderListSize, 200, init.fileLoaderListSize );
    m_pFileLoaders->AddRef();
    m_pDataLoaders = new RefHashTable<radLoadDataLoader>( init.dataLoaderListSize, 200, init.dataLoaderListSize );
    m_pDataLoaders->AddRef();
    m_pLoadQueue = new RefQueue<radLoadObject>( init.loadQueueSize );
    m_pLoadQueue->AddRef();
    m_pCallbacks = new RefQueue<radLoadCallback>( 32 );
    m_pCallbacks ->AddRef();

    ::radThreadCreateMutex( &m_pMutex );
    m_pMutex->Lock();
    ::radThreadCreateThread( &m_pThread, radLoadManager::LoadThreadEntry, static_cast<void*>(this), IRadThread::PriorityNormal, init.loadThreadStackSize );

#ifdef RADLOAD_GATHER_STATS
#ifdef RADLOAD_USE_WATCHER
    radDbgWatchAddUnsignedInt( &m_totalLoads, "Total Loads", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_completedLoads, "Completed Loads", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_pendingLoads, "Pending Loads", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_maxPendingLoads, "Max Pending Loads", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_minLoadTime, "Min Load Time", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_maxLoadTime, "Max Load Time", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_avgLoadTime, "Average Load Time", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_minQueuedTime, "Min Queued Time", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_maxQueuedTime, "Max Queued Time", "radload\\stats", 0, 0 );
    radDbgWatchAddUnsignedInt( &m_avgQueuedTime, "Average Queued Time", "radload\\stats", 0, 0 );
#endif
#endif
}

radLoadManager::~radLoadManager()
{
    // Destruction of objects happens in the Terminate function
}

void radLoadManager::AddCallback( radLoadCallback* callback )
{
    if( callback )
    {
        callback->AddRef();
        if( !IsLoadPending() )
        {
            O3_POOL(Streaming,Callbacks);
        callback->Done();
            callback->Release();
        }
        else
        {
            m_pLoadQueue->Push( callback );
        }
    }
}

void radLoadManager::AddDataLoader( radLoadDataLoader* dataLoader, radLoadClassID id )
{
    m_pDataLoaders->Store( id, dataLoader );
}

void radLoadManager::AddFileLoader( radLoadFileLoader* fileLoader, const char* extension )
{
    m_pFileLoaders->Store( radMakeCaseInsensitiveKey( extension ), fileLoader );
}

void radLoadManager::Cancel()
{
    unsigned int i = 0;

    while( !m_pLoadQueue->Empty() )
    {
       radLoadObject* obj = m_pLoadQueue->Pop();
       radLoadUpdatableRequest* req = dynamic_cast<radLoadUpdatableRequest*>( obj );
       if( req )
       {
           req->SetState( CANCELED );
       }
       obj->Release();
    }
    if( m_pCurrent )
    {
        m_pCurrent->Cancel();
    }
}

radLoadDataLoader* radLoadManager::GetDataLoader( radLoadClassID id )
{
    return m_pDataLoaders->Find( id );
}

radLoadFileLoader* radLoadManager::GetFileLoader( const char* extension )
{
    const char* ext = extension;
    if( *ext == '.' )
    {
        ext++;
    }
    return m_pFileLoaders->Find( radMakeCaseInsensitiveKey( ext ) );
}

#ifdef RAD_3DS
extern "C" volatile int g_radLoadMainWaiting = 0;

static volatile bool s_lent = false;
#endif

void radLoadManager::InternalService()
{
    m_pMutex->Lock();
    while( !m_bDone )
    {
        if( !m_pLoadQueue->Empty() )
        {
            radLoadObject* obj = m_pLoadQueue->Pop();
            radLoadCallback* callback = dynamic_cast<radLoadCallback*>( obj );
            if( callback )
            {
                m_pCallbacks->Push(callback);
            }
            else
            {
                QueueItem* item = dynamic_cast<QueueItem*>(obj);
                if( item )
                {
                    m_pCurrent = item;
                    m_pCurrent->AddRef();
                    m_pCurrent->SetState( LOADING );

                    char* filename = item->GetOptions()->filename;

                    // Find the "." in filename.
                    // Review: RAD: There are lots of functions that do this!  Try:
                    /* 
                    char* extension = strrchr( filename, '.' );
                    rAssert( extension != NULL );
                    extension++;
                    */

                    // Find the file extension.
                    int i = strlen( filename ) - 1;
                    while( i && (filename[i] != '.') )
                    {
                        i--;
                    }
                    i++;

                    radLoadFileLoader* loader = m_pFileLoaders->Find( radMakeCaseInsensitiveKey( filename + i ) );
                    rAssert( loader );
                    if( loader == NULL )
                    {
                        m_pCurrent->SetState( COMPLETE );
                        radLoadObject::Release( m_pCurrent );
                        m_pCurrent = NULL;
                        continue;
                    }
                    radMemoryAllocator old = ::radMemorySetCurrentAllocator (item->GetOptions()->allocator);

                    {
#ifdef SRR2_OLD3DS_NATIVE_BENCH
                    WorkerRegion region(filename);
#endif
                    loader->LoadFile( item->GetOptions(), static_cast<radLoadUpdatableRequest*>( item ) );
                    }
                    
                    ::radMemorySetCurrentAllocator (old);
                    if( m_pCurrent->GetState() == LOADING )
                    {
                        m_pCurrent->SetState( COMPLETE );
                    }
    #ifdef RADLOAD_GATHER_STATS
                    if( m_pCurrent->GetState() == COMPLETE )
                    {
                        m_completedLoads++;
                        unsigned int time = m_pCurrent->GetTotalLoadTime();
                        unsigned int queued = m_pCurrent->GetTotalQueuedTime();
                    
                        m_minLoadTime = (m_minLoadTime > time) ? time : m_minLoadTime;
                        m_maxLoadTime = (m_maxLoadTime < time) ? time : m_maxLoadTime;
                        m_avgLoadTime = (((m_avgLoadTime * (m_totalLoads - 1))/(m_totalLoads)) +
                                (time/m_totalLoads));

                        m_minQueuedTime = (m_minQueuedTime > queued) ? queued : m_minQueuedTime;
                        m_maxQueuedTime = (m_maxQueuedTime < queued) ? queued : m_maxQueuedTime;
                        m_avgQueuedTime = (((m_avgQueuedTime * (m_totalLoads - 1))/(m_totalLoads)) +
                                (queued/m_totalLoads));
                    }
                    m_pendingLoads--;
    #endif
                    radLoadObject::Release( m_pCurrent );
                }
            }
        }
        else
        {
            SwitchTasks();
        }
    }
    m_pMutex->Unlock();
}

bool radLoadManager::IsLoadPending()
{
    return (!m_pLoadQueue->Empty() || m_pCurrent);
}

bool radLoadManager::IsSyncLoading()
{
    return m_bSyncLoading;
}

void radLoadManager::Load( radLoadOptions* options, radLoadRequest** request )
{
#ifdef RADLOAD_GATHER_STATS
    m_totalLoads++;
    m_pendingLoads++;
    m_maxPendingLoads = (m_pendingLoads > m_maxPendingLoads) ? m_pendingLoads : m_maxPendingLoads;
#endif
    rAssert( options );
    rAssert( options->filename );
    options->syncLoad |= m_bSyncLoading;
    radMemoryAllocator old = ::radMemorySetCurrentAllocator( RADMEMORY_ALLOC_TEMP );
    QueueItem* item = new QueueItem( *options );
    ::radMemorySetCurrentAllocator( old );
    item->AddRef();
    m_pLoadQueue->Push( item );
    item->SetState( QUEUED );
    *request = static_cast<radLoadRequest*>(item);
    if( options->stream )
    {
        item->SetStream( options->stream );
    }
    if( options->syncLoad )
    {
        while( item->GetState() != COMPLETE )
        {
            SwitchTasks();
            radFileService();
        }
    }
}

void radLoadManager::Load( const char* filename, radLoadRequest** request )
{
    radLoadOptions options;
    options.filename = new char[strlen( filename )];
    strcpy( options.filename, filename );
    Load( &options, request );
}

unsigned int radLoadManager::LoadThreadEntry( void* data )
{
    radLoadManager* manager = static_cast<radLoadManager*>( data );
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    __atomic_store_n(&manager->m_nativeSeekWorker,getThreadLocalStorage(),__ATOMIC_RELEASE);
#endif
    manager->InternalService();
    return 0;
}

float radLoadManager::PercentDone()
{
    if( m_pLoadQueue->Empty() )
    {
        return 1.0f;
    }
    // While hardly the most accurate number out there, it'll work for now.
    return 1.0f / static_cast<float>(m_pLoadQueue->Size());
}

void radLoadManager::PrintStats()
{
#ifdef RADLOAD_GATHER_STATS
    rDebugPrintf( "RadLoad Stats\n" );
    rDebugPrintf( "=====================================\n" );
    rDebugPrintf( "\tTotal Loads : %d\n", m_totalLoads );
    rDebugPrintf( "\tCompleted Loads : %d\n", m_completedLoads );
    rDebugPrintf( "\tPending Loads : %d\n", m_pendingLoads );
    rDebugPrintf( "\tMax Pending Loads : %d\n", m_maxPendingLoads );
    rDebugPrintf( "\tMin Load Time : %d\n", m_minLoadTime );
    rDebugPrintf( "\tMax Load Time : %d\n", m_maxLoadTime );
    rDebugPrintf( "\tAverage Load Time : %d\n", m_avgLoadTime );
    rDebugPrintf( "\tMin Queued Time : %d\n", m_minQueuedTime );
    rDebugPrintf( "\tMax Queued Time : %d\n", m_maxQueuedTime );
    rDebugPrintf( "\tAverage Queued Time : %d\n", m_avgQueuedTime );
    rDebugPrintf( "=====================================\n" );
#endif
}

void radLoadManager::RemoveDataLoader( radLoadClassID id )
{
    radLoadDataLoader* loader = GetDataLoader( id );
    while( loader )
    {
        RemoveDataLoader( loader );
        loader = GetDataLoader( id );
    }
}

void radLoadManager::RemoveDataLoader( radLoadDataLoader* loader )
{
    m_pDataLoaders->Remove( loader );
}

void radLoadManager::RemoveFileLoader( const char* extension )
{
    radLoadFileLoader* loader = GetFileLoader( extension );
    while( loader )
    {
        RemoveFileLoader( loader );
        loader = GetFileLoader( extension );
    }
}

void radLoadManager::RemoveFileLoader( radLoadFileLoader* loader )
{
    m_pFileLoaders->Remove( loader );
}

void radLoadManager::Service()
{
    if( IsLoadPending() )
    {
        SwitchTasks();
    }

#ifdef RAD_3DS
    if( s_lent )
    {
        return;
    }
#endif

    if(!m_pCallbacks->Empty())
    {
        NB_SCOPE(LoadCallback);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        WorkerRegion region("completion_callback");
#endif
        radLoadCallback* callback = m_pCallbacks->Pop();
        callback->Done();
        callback->Release();
    }


}

void radLoadManager::SetSyncLoading( bool sync )
{
    m_bSyncLoading = sync;
}

void radLoadManager::SwitchTasks()
{
    NB_SCOPE(LoadHandoff);
    O3_POOL(Streaming,LoadHandoff);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    PauseWorkerTiming();
#endif
    m_pMutex->Unlock();
    { NB_SCOPE(LoadYield); radThreadSleep(0); }
    { NB_SCOPE(LoadLock); m_pMutex->Lock(); }
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    ResumeWorkerTiming();
#endif
}

#ifdef SRR2_OLD3DS_NATIVE_BENCH
bool radLoadManager::BeginNativeSeek()
{
    // This worker holds the mutex throughout LoadFile except at explicit yields.
    // A native FILE seek has no engine callback and touches no published object.
    if(__atomic_load_n(&m_nativeSeekWorker,__ATOMIC_ACQUIRE)!=getThreadLocalStorage() ||
       radNativeSeek::owner || !m_pCurrent || m_bDone)return false;
    radNativeSeek::owner=this;radNativeSeek::started=svcGetSystemTick();
    ++radNativeSeek::begun;
    PauseWorkerTiming();
    m_pMutex->Unlock();
    return true;
}
void radLoadManager::EndNativeSeek()
{
    // The current request retains its reference, and this manager outlives its
    // worker. Reclaim this exact mutex even if another global manager is selected.
    m_pMutex->Lock();
    const u64 span=svcGetSystemTick()-radNativeSeek::started;
    radNativeSeek::totalOutside+=span;
    if(span>radNativeSeek::maxOutside)radNativeSeek::maxOutside=span;
    ++radNativeSeek::completed;radNativeSeek::owner=nullptr;
    ResumeWorkerTiming();
}
#endif

void radLoadManager::LendDuringWait()
{
#ifdef RAD_3DS
    if( s_lent || !IsLoadPending() )
    {
        return;
    }

    s_lent = true;
    m_pMutex->Unlock();
#endif
}

void radLoadManager::ReclaimAfterWait()
{
#ifdef RAD_3DS
    if( !s_lent )
    {
        return;
    }

    g_radLoadMainWaiting = 1;
    m_pMutex->Lock();
    g_radLoadMainWaiting = 0;
    s_lent = false;
#endif
}

void radLoadManager::Terminate()
{
    m_pFileLoaders->Release();
    m_pDataLoaders->Release();
    rAssert( m_pLoadQueue->Empty() );
    m_pLoadQueue->Release();
    m_pCallbacks->Release();
    m_bDone = true;
    m_pMutex->Unlock();
    m_pThread->WaitForTermination();
    m_pThread->Release();

    m_pMutex->Release();

    delete this;
}
    
