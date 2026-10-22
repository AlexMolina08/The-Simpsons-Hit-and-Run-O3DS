#ifndef PORTSETTINGS_H
#define PORTSETTINGS_H

namespace PortSettings
{
    bool NoTimers();
    void SetNoTimers( bool enabled );

    bool GraphicsQuality(unsigned group, bool defaultValue);
    void SetGraphicsQuality(unsigned group, bool enabled);
    bool Stereo3D(unsigned group);
    void SetStereo3D(unsigned group, bool enabled);

    void Save();
}
#endif
