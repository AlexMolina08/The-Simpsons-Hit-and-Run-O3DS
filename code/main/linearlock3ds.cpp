#ifdef RAD_3DS
#include <3ds.h>
#include <main/old3dsmemory.h>

extern "C"
{
    void*  __real_linearAlloc( size_t size );
    void*  __real_linearMemAlign( size_t size, size_t alignment );
    void*  __real_linearRealloc( void* mem, size_t size );
    size_t __real_linearGetSize( void* mem );
    void   __real_linearFree( void* mem );
    u32    __real_linearSpaceFree( void );
}

namespace
{
    const u32 LINEAR_KEEP = 16 * 1024;

    RecursiveLock s_Lock;
    bool          s_LockReady = false;
    bool          s_PoolLive = false;

    struct Guard
    {
        Guard( )
        {
            if( !s_LockReady )
            {
                RecursiveLock_Init( &s_Lock );
                s_LockReady = true;
            }
            RecursiveLock_Lock( &s_Lock );
        }
        ~Guard( ) { RecursiveLock_Unlock( &s_Lock ); }
    };

    void RecordLinear(bool failed)
    {
#ifdef SRR2_OLD3DS_MEMORY_DIAG
        Old3dsMemoryLinear(__real_linearSpaceFree(),failed);
#else
        (void)failed;
#endif
    }

    bool WouldEmptyPool( size_t size, size_t alignment )
    {
        return s_PoolLive && (u64) size + alignment + LINEAR_KEEP > __real_linearSpaceFree( );
    }
}

extern "C" void* __wrap_linearAlloc( size_t size )
{
    Guard g;
    if( WouldEmptyPool( size, 0x80 ) )
    {
        RecordLinear(true);
        return NULL;
    }
    void* p = __real_linearAlloc( size );
    RecordLinear(!p && size!=0);
    s_PoolLive = s_PoolLive || p != NULL;
    return p;
}

extern "C" void* __wrap_linearMemAlign( size_t size, size_t alignment )
{
    Guard g;
    if( WouldEmptyPool( size, alignment ) )
    {
        RecordLinear(true);
        return NULL;
    }
    void* p = __real_linearMemAlign( size, alignment );
    RecordLinear(!p && size!=0);
    s_PoolLive = s_PoolLive || p != NULL;
    return p;
}

extern "C" void* __wrap_linearRealloc( void* mem, size_t size )
{
    Guard g;
    void* p=__real_linearRealloc( mem, size );
    RecordLinear(!p && size!=0);
    return p;
}

extern "C" size_t __wrap_linearGetSize( void* mem )
{
    Guard g;
    return __real_linearGetSize( mem );
}

extern "C" void __wrap_linearFree( void* mem )
{
    Guard g;
    __real_linearFree( mem );
}

extern "C" u32 __wrap_linearSpaceFree( void )
{
    Guard g;
    return __real_linearSpaceFree( );
}
#endif
