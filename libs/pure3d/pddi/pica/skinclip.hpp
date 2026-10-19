#ifndef PICA_SKINCLIP_HPP
#define PICA_SKINCLIP_HPP
#include <pddi/pica/skincore.hpp>
#include <cmath>
namespace picaSkinClip {
const unsigned MaxBones=25, Budget=128*1024;
struct Bone { float low[3], high[3]; bool used; };
struct Bounds { Bone bone[MaxBones]; unsigned count; bool valid; };
inline Bounds Build(const picaSkin::Source* source,unsigned vertices,unsigned bones)
{
    Bounds result={};
    if(!source || !vertices || !bones || bones>MaxBones) return result;
    result.count=bones;
    for(unsigned v=0;v<vertices;++v) {
        const picaSkin::Source& s=source[v];
        for(unsigned a=0;a<3;++a)
            if(!std::isfinite(s.position[a]) || std::fabs(s.position[a])>1024) return result;
        float weights[4]={s.weights[0],s.weights[1],s.weights[2],1.0f-(s.weights[2]+s.weights[1]+s.weights[0])};
        if(s.weights[0]==1) {weights[1]=weights[2]=weights[3]=0;}
        double sum=0;
        for(unsigned j=0;j<4;++j) {
            if(!std::isfinite(weights[j]) || weights[j]<0 || weights[j]>1) return result;
            sum+=weights[j];
            if((j==0 || s.weights[0]!=1) && s.indices[j]>=bones) return result;
            if(!weights[j]) continue;
            Bone& b=result.bone[s.indices[j]];
            for(unsigned a=0;a<3;++a) {
                if(!b.used || s.position[a]<b.low[a]) b.low[a]=s.position[a];
                if(!b.used || s.position[a]>b.high[a]) b.high[a]=s.position[a];
            }
            b.used=true;
        }
        if(std::fabs(sum-1)>0.00001) return result;
    }
    result.valid=true;
    return result;
}
inline bool Merge(Bounds& destination,const Bounds& source)
{
    if(!source.valid || !source.count || source.count>MaxBones) return false;
    if(!destination.valid) {destination=source;return true;}
    if(source.count>destination.count) destination.count=source.count;
    for(unsigned i=0;i<source.count;++i) {
        const Bone& input=source.bone[i];Bone& output=destination.bone[i];
        if(!input.used) continue;
        if(!output.used) {output=input;continue;}
        for(unsigned a=0;a<3;++a) {
            if(input.low[a]<output.low[a]) output.low[a]=input.low[a];
            if(input.high[a]>output.high[a]) output.high[a]=input.high[a];
        }
    }
    return true;
}
// Matrices are copied in the CPU skin's row-vector convention. Every input
// matrix is checked, including zero-weight influences used by the reference.
inline bool Transform(const Bounds& bounds,const float matrices[MaxBones][4][4],
                      unsigned count,float low[3],float high[3])
{
    if(!bounds.valid || count>MaxBones || count!=bounds.count) return false;
    bool first=true;
    for(unsigned i=0;i<count;++i) {
        const float (*m)[4]=matrices[i];
        for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c)
            if(!std::isfinite(m[r][c]) || std::fabs(m[r][c])>(r==3?1000000.0f:16.0f)) return false;
        if(m[0][3]!=0 || m[1][3]!=0 || m[2][3]!=0 || m[3][3]!=1) return false;
        const Bone& b=bounds.bone[i];
        if(!b.used) continue;
        for(unsigned a=0;a<3;++a) {
            float lower=m[3][a], upper=lower, magnitude=std::fabs(lower);
            for(unsigned j=0;j<3;++j) {
                const float x=m[j][a]*b.low[j], y=m[j][a]*b.high[j];
                lower+=x<y?x:y; upper+=x>y?x:y;
                const float ax=std::fabs(x), ay=std::fabs(y);
                magnitude+=ax>ay?ax:ay;
            }
            // Enclose float32/float24 skin operations, packed position error
            // (at most 1/16384 per axis), and non-exact weight sums. The 0.01
            // absolute term covers packed near-zero coordinates at scale <=16.
            const float margin=0.01f+0.002f*magnitude;
            lower-=margin;upper+=margin;
            if(first || lower<low[a]) low[a]=lower;
            if(first || upper>high[a]) high[a]=upper;
        }
        first=false;
    }
    return !first;
}
}
#endif
