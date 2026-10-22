#include <cheats/portsettings.h>

#include <stdio.h>
#include <string.h>

namespace
{
#ifdef RAD_3DS
    const char* SETTINGS_FILE = "sdmc:/3ds/simpsons/srr2_settings.ini";
#else
    const char* SETTINGS_FILE = "srr2_settings.ini";
#endif

    bool s_Loaded   = false;
    bool s_NoTimers = false;
    int s_GraphicsQuality[3] = {-1, -1, -1};

    void LoadOnce()
    {
        if( s_Loaded )
        {
            return;
        }
        s_Loaded = true;

        FILE* f = fopen( SETTINGS_FILE, "r" );
        if( f == NULL )
        {
            return;
        }

        char line[ 64 ];
        while( fgets( line, sizeof( line ), f ) != NULL )
        {
            int value = 0;
            if( sscanf( line, "notimers=%d", &value ) == 1 )
            {
                s_NoTimers = ( value != 0 );
            }

            unsigned group = 0;
            if(sscanf(line, "graphics_quality_%u=%d", &group, &value) == 2 &&
               group < 3 && (value == 0 || value == 1)) s_GraphicsQuality[group] = value;

        }
        fclose( f );
    }
}

bool PortSettings::NoTimers()
{
    LoadOnce();
    return s_NoTimers;
}

void PortSettings::SetNoTimers( bool enabled )
{
    LoadOnce();
    s_NoTimers = enabled;
}

void PortSettings::Save()
{
    LoadOnce();

    FILE* f = fopen( SETTINGS_FILE, "w" );
    if( f == NULL )
    {
        return;
    }
    fprintf( f, "notimers=%d\n", s_NoTimers ? 1 : 0 );
    for(unsigned group = 0; group < 3; ++group)
        if(s_GraphicsQuality[group] >= 0)
            fprintf(f, "graphics_quality_%u=%d\n", group, s_GraphicsQuality[group]);
    fclose( f );
}

bool PortSettings::GraphicsQuality(unsigned group, bool defaultValue)
{
    LoadOnce();
    return group < 3 && s_GraphicsQuality[group] >= 0 ? s_GraphicsQuality[group] != 0 : defaultValue;
}

void PortSettings::SetGraphicsQuality(unsigned group, bool enabled)
{
    LoadOnce();
    if(group < 3) s_GraphicsQuality[group] = enabled ? 1 : 0;
}
