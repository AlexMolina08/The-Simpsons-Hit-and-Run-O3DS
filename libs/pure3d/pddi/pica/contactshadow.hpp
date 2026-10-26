#ifndef PICA_CONTACTSHADOW_HPP
#define PICA_CONTACTSHADOW_HPP
#include <cmath>
#include <cstdint>

namespace picaContact {
#ifdef SRR2_OLD3DS_NATIVE_BENCH
extern unsigned queued,batches,drawn,overflow,commandSkips;
#endif
constexpr unsigned Capacity=12,Vertices=17,Indices=72;
struct Vertex { float x,y,alpha; unsigned char offset[4]; };
struct Instance { float center[4],normal[4],facing[4]; };
static_assert(sizeof(Vertex)==16,"Contact shadow stream");
inline bool GroundY(float& y,const float* center,const float* point,const float* normal)
{
    for(unsigned i=0;i<3;++i)
        if(!std::isfinite(center[i]) || !std::isfinite(point[i]) || !std::isfinite(normal[i]))return false;
    if(normal[1]<.3f)return false;
    y=point[1]-((center[0]-point[0])*normal[0]+(center[2]-point[2])*normal[2])/normal[1];
    return std::isfinite(y);
}
inline float Depth(const float* view,const float* point)
{
    // Matrix::SetPerspective sets Row4(2).w=1. PointCamera faces target-position;
    // positive eye-space Z is forward. PICA flips depth Z but preserves clip W.
    return point[0]*view[2]+point[1]*view[6]+point[2]*view[10]+view[14];
}
inline bool Prepare(Instance& out,const float* center,const float* normal,const float* facing,
                    float width,float length,float alpha)
{
    for(unsigned i=0;i<3;++i)
        if(!std::isfinite(center[i]) || !std::isfinite(normal[i]) || !std::isfinite(facing[i]))return false;
    if(!(width>0 && width<10 && length>0 && length<10 && alpha>0 && alpha<=1) || normal[1]<.3f)return false;
    for(unsigned i=0;i<3;++i){out.center[i]=center[i]+normal[i]*.025f;out.normal[i]=normal[i];out.facing[i]=facing[i];}
    out.center[3]=alpha;out.normal[3]=width;out.facing[3]=length;
    return true;
}
inline void Mesh(Vertex* vertices,unsigned short* indices)
{
    constexpr float circle[8][2]={{1,0},{.70710678f,.70710678f},{0,1},{-.70710678f,.70710678f},
        {-1,0},{-.70710678f,-.70710678f},{0,-1},{.70710678f,-.70710678f}};
    for(unsigned instance=0;instance<Capacity;++instance)
    {
        const unsigned base=instance*Vertices,index=instance*Indices;
        for(unsigned v=0;v<Vertices;++v)
        {
            const unsigned point=(v-1)%8;
            const float radius=v<=8?.62f:1.0f;
            vertices[base+v]={v?circle[point][0]*radius:0,v?circle[point][1]*radius:0,
                v<=8?1.0f:0.0f,{static_cast<unsigned char>(instance*3),0,0,0}};
        }
        for(unsigned p=0;p<8;++p)
        {
            const unsigned a=base+1+p,b=base+1+(p+1)%8,c=base+9+p,d=base+9+(p+1)%8;
            const unsigned values[]={base,a,b,a,c,b,b,c,d};
            for(unsigned i=0;i<9;++i)indices[index+p*9+i]=values[i];
        }
    }
}
}
extern "C" void picaContactBegin();
extern "C" void picaContactQueue(const float* center,const float* normal,const float* facing,float width,float length,float alpha);
extern "C" void picaContactDraw();
#endif
