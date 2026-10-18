#ifndef SIM_INERTIA_TENSOR_CACHE_HPP
#define SIM_INERTIA_TENSOR_CACHE_HPP
#include <simphysics/jointvectorcache.hpp>
#include <cmath>
#include <cstring>
namespace sim { namespace inertiaTensorCache {
// Cache only the local tensor rotation. Center of mass, point-mass terms,
// accumulation, inversion and the original update cadence remain unchanged.
class Cache {
    float rotation[3][3],source[6],result[6];
    unsigned control;
    bool valid=false;
public:
    template<class Tensor,class Matrix> bool Apply(Tensor& output,const Tensor& input,const Matrix& matrix)
    {
        const float values[6]={input.xx,input.xy,input.xz,input.yy,input.yz,input.zz};
        const unsigned current=jointVectorCache::Control();
        // Reject a changed world rotation before checking the local tensor,
        // which normally stays constant between physics-property changes.
        if(valid && current==control &&
           !std::memcmp(matrix.m[0],rotation[0],12) && !std::memcmp(matrix.m[1],rotation[1],12) &&
           !std::memcmp(matrix.m[2],rotation[2],12) && !std::memcmp(values,source,sizeof(source)))
        {
            output.xx=result[0];output.xy=result[1];output.xz=result[2];
            output.yy=result[3];output.yz=result[4];output.zz=result[5];
            return true;
        }
        output=input;
        output.TransformSelf(matrix);
        valid=std::isfinite(output.xx) && std::isfinite(output.xy) && std::isfinite(output.xz) &&
              std::isfinite(output.yy) && std::isfinite(output.yz) && std::isfinite(output.zz);
        for(unsigned i=0;i<6;++i) valid=valid && std::isfinite(values[i]);
        for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c)
            valid=valid && std::isfinite(matrix.m[r][c]);
        if(valid)
        {
            std::memcpy(source,values,sizeof(source));control=current;
            for(unsigned r=0;r<3;++r) std::memcpy(rotation[r],matrix.m[r],12);
            result[0]=output.xx;result[1]=output.xy;result[2]=output.xz;
            result[3]=output.yy;result[4]=output.yz;result[5]=output.zz;
        }
        return false;
    }
};
static_assert(sizeof(Cache)==92,"Bounded local inertia tensor cache");
}}
#endif
