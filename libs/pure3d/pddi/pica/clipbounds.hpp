#ifndef PICA_CLIP_BOUNDS_HPP
#define PICA_CLIP_BOUNDS_HPP
#include <cmath>
namespace picaClipBounds {
struct Transform {
    float coefficient[4][4], error[4][4];
    bool valid;
};
#ifdef SRR2_OLD3DS_CLIP_PLANE_PREP
struct ProjectionPlanes
{
    float weight[4][4], magnitude[4][4];
    unsigned char indices[4][4], count[4];
    bool valid;
};
inline ProjectionPlanes PrepareProjection(const float projection[4][4])
{
    ProjectionPlanes result={};
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c)
        if(!std::isfinite(projection[r][c]) || std::fabs(projection[r][c])>10000.0f) return result;
    for(unsigned plane=0;plane<4;++plane)
    {
        const unsigned axis=plane/2;
        const float sign=plane%2 ? -1.0f : 1.0f;
        for(unsigned k=0;k<4;++k)
        {
            const float weight=projection[3][k]+sign*projection[axis][k];
            const float magnitude=std::fabs(projection[3][k])+std::fabs(projection[axis][k]);
            // Retain cancelling coefficients when their quantization bound is nonzero.
            if(weight!=0 || magnitude!=0)
            {
                const unsigned term=result.count[plane]++;
                result.indices[plane][term]=k;
                result.weight[plane][term]=weight;
                result.magnitude[plane][term]=magnitude;
            }
        }
    }
    result.valid=true;
    return result;
}
inline Transform Prepare(const float modelView[4][4], const ProjectionPlanes& projection)
{
    Transform result={};
    if(!projection.valid) return result;
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c)
        if(!std::isfinite(modelView[r][c]) ||
           std::fabs(modelView[r][c])>(c==3 ? 1000000.0f : 16.0f)) return result;
    if(modelView[3][0]!=0 || modelView[3][1]!=0 || modelView[3][2]!=0 || modelView[3][3]!=1) return result;
    for(unsigned plane=0;plane<4;++plane) for(unsigned j=0;j<4;++j)
    {
        float coefficient=0, magnitude=0;
        // Original k order and arithmetic remain; identically zero terms vanish.
        for(unsigned term=0;term<projection.count[plane];++term)
        {
            const unsigned k=projection.indices[plane][term];
            coefficient+=projection.weight[plane][term]*modelView[k][j];
            magnitude+=projection.magnitude[plane][term]*std::fabs(modelView[k][j]);
        }
        result.coefficient[plane][j]=coefficient;
        result.error[plane][j]=magnitude*0.001f;
    }
    result.valid=true;
    return result;
}
inline Transform Prepare(const float modelView[4][4], const float projection[4][4])
{
    return Prepare(modelView,PrepareProjection(projection));
}
#else
inline Transform Prepare(const float modelView[4][4], const float projection[4][4])
{
    Transform t = {};
    // Bound intermediates below float24 overflow; subnormal contributions fit
    // inside the absolute rejection margin. Unsupported matrices stay visible.
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) {
        if(!std::isfinite(modelView[r][c]) || !std::isfinite(projection[r][c]) ||
           std::fabs(modelView[r][c]) > (c==3 ? 1000000.0f : 16.0f) ||
           std::fabs(projection[r][c]) > 10000.0f) return t;
    }
    if(modelView[3][0]!=0 || modelView[3][1]!=0 || modelView[3][2]!=0 || modelView[3][3]!=1) return t;
    for(unsigned plane=0;plane<4;++plane) {
        const unsigned axis=plane/2;
        const float sign=plane%2 ? -1.0f : 1.0f;
        for(unsigned j=0;j<4;++j) {
            float coefficient=0, magnitude=0;
            for(unsigned k=0;k<4;++k) {
                coefficient += (projection[3][k]+sign*projection[axis][k])*modelView[k][j];
                magnitude += (std::fabs(projection[3][k])+std::fabs(projection[axis][k]))*std::fabs(modelView[k][j]);
            }
            t.coefficient[plane][j]=coefficient;
            // Covers input/uniform quantization, both dot products, plane
            // addition and CPU coefficient rounding. Deliberately wider than
            // twelve float24 round/truncate errors (12 / 65536 < 0.000184).
            t.error[plane][j]=magnitude*0.001f;
        }
    }
    t.valid=true;
    return t;
}
#endif
// Zero means visible/unsupported; otherwise every source point lies outside
// this one homogeneous side plane, including the float24 arithmetic margin.
inline unsigned Reject(const Transform& t,const float low[3],const float high[3])
{
    if(!t.valid) return 0;
    float absolute[3];
    for(unsigned axis=0;axis<3;++axis) {
        if(!std::isfinite(low[axis]) || !std::isfinite(high[axis]) || low[axis]>high[axis]) return 0;
        const float a=std::fabs(low[axis]), b=std::fabs(high[axis]);
        absolute[axis]=a>b?a:b;
        if(absolute[axis]>1000000.0f) return 0;
    }
    for(unsigned plane=0;plane<4;++plane) {
        float upper=t.coefficient[plane][3];
        float error=t.error[plane][3]+0.0001f;
        for(unsigned axis=0;axis<3;++axis) {
            const float c=t.coefficient[plane][axis];
            upper += c*(c<0 ? low[axis] : high[axis]);
            error += t.error[plane][axis]*absolute[axis];
        }
        if(upper < -error) return plane+1;
    }
    return 0;
}
}
#endif
