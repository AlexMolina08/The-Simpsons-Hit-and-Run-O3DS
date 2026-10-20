#ifndef P3D_PARTICLE_BOUNDS_HPP
#define P3D_PARTICLE_BOUNDS_HPP
#include <cmath>
#include <cfloat>
namespace p3dParticleBounds {
// Actual live-particle bounds at Display, after all original updates and
// cohesion. No extrapolated lifetime or generator envelope is assumed.
struct Bounds {
    float low[3]={},high[3]={},previousLow[3]={},previousHigh[3]={};
    float size=0,spin=0;unsigned count=0;bool valid=false;
    void Begin(){count=0;size=spin=0;valid=true;}
    void Add(const float* current,const float* previous,float radius,float angle)
    {
        for(unsigned axis=0;axis<3;++axis)
            if(!(std::fabs(current[axis])<=1000000.f && std::fabs(previous[axis])<=1000000.f)){valid=false;return;}
        if(!(std::fabs(radius)<=500000.f)){valid=false;return;}
        if(!std::isfinite(angle))spin=INFINITY;
        else if(std::fabs(angle)>spin)spin=std::fabs(angle);
        if(std::fabs(radius)>size)size=std::fabs(radius);
        for(unsigned axis=0;axis<3;++axis){
            if(!count){low[axis]=high[axis]=current[axis];previousLow[axis]=previousHigh[axis]=previous[axis];}
            else {
                if(current[axis]<low[axis])low[axis]=current[axis];if(current[axis]>high[axis])high[axis]=current[axis];
                if(previous[axis]<previousLow[axis])previousLow[axis]=previous[axis];if(previous[axis]>previousHigh[axis])previousHigh[axis]=previous[axis];
            }
        }
        ++count;
    }
};
inline bool Transform(const float matrix[4][4],const float* low,const float* high,float* outLow,float* outHigh)
{
    for(unsigned axis=0;axis<3;++axis){
        float minimum=matrix[3][axis],maximum=minimum,absolute=std::fabs(minimum);
        if(!(absolute<=1000000.f))return false;
        for(unsigned row=0;row<3;++row){
            if(!(std::fabs(matrix[row][axis])<=16.f))return false;
            const float a=matrix[row][axis]*low[row],b=matrix[row][axis]*high[row];
            minimum+=a<b?a:b;maximum+=a>b?a:b;
            const float aa=std::fabs(a),bb=std::fabs(b);absolute+=aa>bb?aa:bb;
        }
        // Covers float32 products/sums and interval endpoints under every
        // rounding direction. The existing native float24 clip margin follows.
        const float margin=absolute*(64.f*FLT_EPSILON)+0.001f;
        outLow[axis]=minimum-margin;outHigh[axis]=maximum+margin;
    }
    return true;
}
inline bool FiniteDepth(const float matrix[4][4],const Bounds& bounds)
{
    float low[3],high[3],oldLow[3],oldHigh[3];
    return bounds.valid && bounds.count && Transform(matrix,bounds.low,bounds.high,low,high) &&
        Transform(matrix,bounds.previousLow,bounds.previousHigh,oldLow,oldHigh) &&
        (low[2]>.001f || high[2]<-.001f) && (oldLow[2]>.001f || oldHigh[2]<-.001f);
}
}
#endif
