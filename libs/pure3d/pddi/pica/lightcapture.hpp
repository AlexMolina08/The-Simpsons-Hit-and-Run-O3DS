#ifndef PICA_LIGHTCAPTURE_HPP
#define PICA_LIGHTCAPTURE_HPP
#include <cstring>
#include <cmath>
namespace picaLightCapture {
// Values only; no light, matrix-stack or asset pointer survives a call.
// Eye-light output has one writer, so an exact input match retains that output.
class Cache
{
    unsigned char matrix[64], source[12];
    unsigned type, control;
    bool valid=false;
public:
    bool Matches(const void* inputMatrix,const void* inputSource,
                 unsigned inputType,unsigned inputControl) const
    {
        return valid && type==inputType && control==inputControl &&
            !std::memcmp(source,inputSource,sizeof(source)) &&
            !std::memcmp(matrix,inputMatrix,sizeof(matrix));
    }
    void Store(const void* inputMatrix,const void* inputSource,
               unsigned inputType,unsigned inputControl,bool finite)
    {
        valid=finite;
        if(!valid) return;
        std::memcpy(matrix,inputMatrix,sizeof(matrix));
        std::memcpy(source,inputSource,sizeof(source));
        type=inputType;control=inputControl;
    }
};
static_assert(sizeof(Cache)<=88,"Bounded per-slot light capture cache");
}
#endif
