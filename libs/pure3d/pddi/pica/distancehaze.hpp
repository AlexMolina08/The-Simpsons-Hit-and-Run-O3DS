#ifndef PICA_DISTANCEHAZE_HPP
#define PICA_DISTANCEHAZE_HPP
#include <cmath>
namespace picaDistanceHaze {
constexpr float Strength = 0.16f;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
extern bool enabled;
extern unsigned builds, passes;
extern float nearPlane, farPlane;
inline bool Enabled() { return enabled; }
#else
inline bool Enabled() { return true; }
#endif
inline float Visibility(float depth, float nearPlane, float farPlane)
{
    const float z = farPlane*nearPlane/(depth*(farPlane-nearPlane)+nearPlane);
    float t = (z-45.0f)/95.0f;
    if(t < 0) t = 0;
    if(t > 1) t = 1;
    return 1.0f - Strength*t*t*(3.0f-2.0f*t);
}
inline bool Build(float* data, float nearPlane, float farPlane)
{
    if(!std::isfinite(nearPlane) || !std::isfinite(farPlane) ||
       nearPlane <= 0 || farPlane <= nearPlane) return false;
    const float product=nearPlane*farPlane;
    if(!std::isfinite(product) || product<=0) return false;
    // Citro3D expects 128 visibility values followed by 128 differences,
    // rather than 256 successive samples. Depth zero is the far plane.
    float previous = Visibility(0,nearPlane,farPlane);
    for(unsigned i=0; i<128; ++i)
    {
        const float next = Visibility(float(i+1)/128.0f,nearPlane,farPlane);
        data[i] = previous;
        data[i+128] = next-previous;
        previous = next;
    }
    return true;
}
}
extern "C" void picaDistanceHazeBegin(unsigned level, unsigned colour);
extern "C" void picaDistanceHazeEnd();
#endif
