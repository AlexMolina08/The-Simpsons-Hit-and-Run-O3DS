#ifndef P3D_SKINZERO_HPP
#define P3D_SKINZERO_HPP
#include <cstdint>
#include <cstring>
namespace p3dSkin
{
// Per draw, so palette mutations between draws cannot leave stale validation.
class ZeroInfluences
{
    unsigned char bounded[256];
    static std::uint32_t Magnitude(float value)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits & 0x7fffffffu;
    }
    static bool Normal(float value)
    {
        const std::uint32_t magnitude = Magnitude(value);
        return magnitude >= 0x00800000u && magnitude < 0x7f800000u;
    }
public:
    ZeroInfluences() { std::memset(bounded, 0, sizeof(bounded)); }
    template<class Vector, class Matrix>
    bool CanSkip(float weight, const Vector& accumulated, const Vector& source,
                 const Matrix& matrix, unsigned index)
    {
        if(weight != 0 || index >= sizeof(bounded)) return false;
        // Adding either signed zero leaves a finite normal accumulator unchanged
        // in every rounding mode. Zero/subnormal/nonfinite accumulators fall back.
        if(!Normal(accumulated.x) || !Normal(accumulated.y) || !Normal(accumulated.z))
            return false;
        // |source| <= 2^20 and |matrix| <= 2^100 keep every multiply and partial
        // sum finite, even with directed rounding. Thus multiplication by zero
        // cannot conceal an overflow, infinity or NaN from the original path.
        if(Magnitude(source.x) > 0x49800000u || Magnitude(source.y) > 0x49800000u ||
           Magnitude(source.z) > 0x49800000u) return false;
        if(!bounded[index])
        {
            bounded[index] = 1;
            for(unsigned row = 0; row < 4; ++row)
                for(unsigned column = 0; column < 3; ++column)
                    if(Magnitude(matrix.m[row][column]) > 0x71800000u)
                        bounded[index] = 2;
        }
        return bounded[index] == 1;
    }
};
static_assert(sizeof(ZeroInfluences) == 256, "Bounded CPU skin validation scratch");
}
#endif
