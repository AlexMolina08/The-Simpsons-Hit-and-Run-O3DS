#ifndef PICA_PARTICLE_CLIP_HPP
#define PICA_PARTICLE_CLIP_HPP
#include <pddi/pica/clipbounds.hpp>
namespace picaParticleClip {
// Sprite corners are (x,y), (-x,-y), (-y,x), (y,-x), where x and y
// are size*cos(angle) and size*sin(angle). Twice the absolute size bounds
// every corner generously, before the shared float24 projection margin.
inline unsigned Reject(const picaClipBounds::Transform& transform,const float center[3],float size,float spin=0)
{
    if(!std::isfinite(spin) || std::fabs(spin)>65536.0f) return 0;
    if(!std::isfinite(size) || std::fabs(size)>500000.0f) return 0;
    const float extent=2.0f*std::fabs(size)+0.0001f;
    const float low[3]={center[0]-extent,center[1]-extent,center[2]};
    const float high[3]={center[0]+extent,center[1]+extent,center[2]};
    return picaClipBounds::Reject(transform,low,high);
}
}
#ifdef SRR2_OLD3DS_PARTICLE_CLIP
extern "C" bool picaPrepareParticleClip(picaClipBounds::Transform* output);
#endif
#endif
