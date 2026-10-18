#ifndef SIM_JOINTVECTORCACHE_HPP
#define SIM_JOINTVECTORCACHE_HPP
#include <cstring>
#ifndef __arm__
#include <cfenv>
#endif
namespace sim { namespace jointVectorCache {
inline unsigned Control()
{
#ifdef __arm__
    unsigned control;
    __asm__ __volatile__("vmrs %0, fpscr" : "=r"(control) : : "memory");
    return control & 0x03c00000u;
#else
    return static_cast<unsigned>(std::fegetround());
#endif
}
// The joint owns its prior matrix and both rotated outputs. Only SetTransform
// writes those outputs after construction. Retain source values, never pointers.
class Cache
{
    unsigned char center[12], axis[12];
    unsigned control;
    bool valid=false;
public:
    bool Matches(const float oldMatrix[4][4], const float newMatrix[4][4],
                 const void* inputCenter, const void* inputAxis, unsigned inputControl) const
    {
        // Rotation changes dominate misses on moving vehicles. Reject those
        // before comparing the usually unchanged local center and joint axis.
        return valid && control==inputControl &&
            !std::memcmp(oldMatrix[0],newMatrix[0],12) &&
            !std::memcmp(oldMatrix[1],newMatrix[1],12) &&
            !std::memcmp(oldMatrix[2],newMatrix[2],12) &&
            !std::memcmp(center,inputCenter,12) && !std::memcmp(axis,inputAxis,12);
    }
    void Store(const void* inputCenter,const void* inputAxis,unsigned inputControl,bool finite)
    {
        valid=finite;
        if(!valid) return;
        std::memcpy(center,inputCenter,12);std::memcpy(axis,inputAxis,12);
        control=inputControl;
    }
};
static_assert(sizeof(Cache)==32,"Bounded joint-vector provenance");
}}
#endif
