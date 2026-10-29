// Resolve existing read-only asset paths whose letter case differs from RomFS.
// The caller supplies the mount root, and retains the exact-open fast path.
#ifndef SRR2_CASEPATH_HPP
#define SRR2_CASEPATH_HPP

#include <dirent.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

static bool srr2ResolveCasePath( const char* root, const char* relative,
                                 char* result, size_t capacity )
{
    const size_t rootLength = strlen( root );
    if( !relative[ 0 ] || !capacity || rootLength >= capacity ||
        !rootLength || root[ rootLength - 1 ] != '/' ) return false;
    memcpy( result, root, rootLength + 1 );
    size_t used = rootLength;
    const char* component = relative;
    while( *component )
    {
        const char* end = strchr( component, '/' );
        const size_t length = end ? (size_t)( end - component ) : strlen( component );
        if( !length || length >= capacity ||
            ( length == 1 && component[ 0 ] == '.' ) ||
            ( length == 2 && component[ 0 ] == '.' && component[ 1 ] == '.' ) ) return false;

        DIR* dir = opendir( result );
        if( !dir ) return false;
        const char* match = NULL;
        bool ambiguous = false;
        char selected[ 257 ];
        struct dirent* entry;
        while( ( entry = readdir( dir ) ) != NULL )
        {
            if( strlen( entry->d_name ) != length ||
                strncasecmp( entry->d_name, component, length ) != 0 ) continue;
            if( length >= sizeof( selected ) ) { ambiguous = true; break; }
            if( memcmp( entry->d_name, component, length ) == 0 )
            {
                memcpy( selected, entry->d_name, length + 1 );
                match = selected;
                ambiguous = false;
                break;
            }
            if( match ) { ambiguous = true; continue; }
            memcpy( selected, entry->d_name, length + 1 );
            match = selected;
        }
        closedir( dir );
        if( !match || ambiguous || used + length + ( end ? 1 : 0 ) >= capacity ) return false;
        memcpy( result + used, match, length );
        used += length;
        if( end ) result[ used++ ] = '/';
        result[ used ] = '\0';
        if( !end ) return true;
        component = end + 1;
        if( !*component ) return false;
    }
    return false;
}
#endif
