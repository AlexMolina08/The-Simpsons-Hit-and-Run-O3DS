#ifndef PICA_VISUALSTYLE_HPP
#define PICA_VISUALSTYLE_HPP

#include <cmath>

namespace picaVisual {
enum Feature { Characters = 1, Vehicles = 2, Atmosphere = 4, Reflections = 8, Occlusion = 16, KeyLighting = 32, ContactShadows = 64, SceneryLighting = 128 };
#if defined(SRR2_OLD3DS_VISUAL_UPGRADE) && defined(SRR2_OLD3DS_NATIVE_BENCH)
extern unsigned features;
extern unsigned reflectionDraws;
extern unsigned sceneryDraws;
inline bool Enabled(Feature feature) { return (features & feature) != 0; }
#else
inline bool Enabled(Feature)
{
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    return true;
#else
    return false;
#endif
}
#endif

// Lift weak neutral paint reflections when loading their material. Strong
// authored reflections, coloured reflections and glass retain their own values.
inline unsigned CoatReflectivity(unsigned strength)
{
    return strength>=96 ? strength : (strength*3<96 ? strength*3 : 96);
}

// Prepare broad, restrained lighting in existing color bytes. No extra vertex
// attributes, recurring skin work, texture lookups or draw calls are required.
inline bool Bake(const float* normal, unsigned char* colour, bool vehicle)
{
    const float lengthSquared = normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2];
    if(!std::isfinite(lengthSquared) || lengthSquared < 0.000001f) return false;
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    const float up = normal[1] * inverseLength;
    const float key = std::fmax(0.0f,
        (-0.40f*normal[0] + 0.80f*normal[1] + 0.4472136f*normal[2]) * inverseLength);
    const float shade = std::fmin(1.0f, vehicle ? 0.76f + 0.20f*up + 0.12f*key
                                               : 0.90f + 0.08f*up + 0.08f*key);
    for(unsigned channel = 0; channel < 3; ++channel)
        colour[channel] = static_cast<unsigned char>(colour[channel] * shade + 0.5f);
    return true;
}
}
#endif
