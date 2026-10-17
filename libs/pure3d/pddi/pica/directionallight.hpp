#ifndef PICA_DIRECTIONAL_LIGHT_HPP
#define PICA_DIRECTIONAL_LIGHT_HPP
#include <cmath>
namespace picaDirectionalLight {
// Directional light distance and attenuation do not depend on vertex position.
// Keep original vectors for any mixed, degenerate or unsupported light set.
// CaptureEyeLight already normalized these vectors. Reuse that result; only
// the redundant float24 normalization disappears. Unit-range failures fall back.
template<class Vector>
inline bool Prepare(const Vector& input, Vector& output)
{
    if(input.w != 0.0f || !std::isfinite(input.x) ||
       !std::isfinite(input.y) || !std::isfinite(input.z)) return false;
    const float squared=(input.x*input.x+input.y*input.y)+input.z*input.z;
    if(!std::isfinite(squared) || squared<0.99999f || squared>1.00001f) return false;
    output=input;
    return true;
}
}
#endif
