#ifndef PICA_CHARACTERFORM_HPP
#define PICA_CHARACTERFORM_HPP

#include <cmath>

namespace picaCharacterForm {
#ifdef SRR2_OLD3DS_NATIVE_BENCH
extern bool enabled;
extern unsigned groups, vertices;
inline bool Enabled() { return enabled; }
#else
inline bool Enabled() { return true; }
#endif
// Bake soft overhead fill into the existing color bytes. This is asset
// preparation, never a skin/update operation. Keep alpha and source geometry.
inline bool Bake(const float* normal, unsigned char* colour)
{
    const float lengthSquared = normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2];
    if(!std::isfinite(lengthSquared) || lengthSquared < 0.000001f)
        return false;
    const float up = normal[1] / std::sqrt(lengthSquared);
    const float shade = 0.82f + 0.18f * up;
    for(unsigned channel = 0; channel < 3; ++channel)
        colour[channel] = static_cast<unsigned char>(colour[channel] * shade + 0.5f);
    return true;
}
}
#endif
