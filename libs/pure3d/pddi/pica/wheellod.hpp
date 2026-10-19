#ifndef PICA_WHEELLOD_HPP
#define PICA_WHEELLOD_HPP
#include <cmath>
#include <cstring>
#include <cstdint>
namespace picaWheelLOD {
const unsigned Copies=4;
const unsigned Budget=256*1024;
const unsigned GroupLimit=32*1024;
const unsigned Reserve=2*1024*1024;
struct Vertex {float position[3],normal[3],uv[2];unsigned char colour[4];};
static_assert(sizeof(Vertex)==36,"Native wheel vertex layout");
struct PaletteVertex {float position[3],normal[4],uv[2];unsigned char colour[4];};
static_assert(sizeof(PaletteVertex)==40,"Rigid palette native layout");
inline PaletteVertex Palette(const Vertex& source,unsigned wheel)
{
    PaletteVertex result;
    memcpy(result.position,source.position,sizeof(source.position));
    memcpy(result.normal,source.normal,sizeof(source.normal));
    result.normal[3]=float(wheel*4);
    memcpy(result.uv,source.uv,sizeof(source.uv));
    memcpy(result.colour,source.colour,sizeof(source.colour));
    return result;
}
inline bool WantFar(bool previous,float distanceSquared)
{
    return std::isfinite(distanceSquared) && distanceSquared>=(previous ? 45.0f*45.0f : 55.0f*55.0f);
}
inline void Transform(const float matrix[4][4],const float input[3],float output[3],bool point)
{
    for(unsigned c=0;c<3;++c)
    {
        output[c]=input[0]*matrix[0][c]+input[1]*matrix[1][c]+input[2]*matrix[2][c];
        if(point) output[c]+=matrix[3][c];
    }
}
inline bool BakeVertex(Vertex& output,const Vertex& source,const float matrix[4][4])
{
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
        if(!std::isfinite(matrix[r][c])) return false;
    for(unsigned c=0;c<3;++c)
        if(!std::isfinite(source.position[c]) || !std::isfinite(source.normal[c])) return false;
    if(!std::isfinite(source.uv[0]) || !std::isfinite(source.uv[1])) return false;
    Vertex result=source;
    Transform(matrix,source.position,result.position,true);
    Transform(matrix,source.normal,result.normal,false);
    for(unsigned c=0;c<3;++c)
        if(!std::isfinite(result.position[c]) || !std::isfinite(result.normal[c])) return false;
    output=result;
    return true;
}
inline unsigned TriangleCount(unsigned indices,bool strip)
{
    if(strip) return indices>=3 ? indices-2 : 0;
    return indices && indices%3==0 ? indices/3 : 0;
}
inline bool Triangle(unsigned short output[3],const unsigned short* indices,unsigned indexCount,
                     unsigned vertices,unsigned copy,unsigned triangle,bool strip)
{
    if(!indices || !vertices || vertices>65535/Copies || copy>=Copies || triangle>=TriangleCount(indexCount,strip)) return false;
    unsigned i[3]={strip ? triangle : triangle*3,strip ? triangle+1 : triangle*3+1,strip ? triangle+2 : triangle*3+2};
    // PICA strips alternate the first two corners. Keep the provoking corner last.
    if(strip && (triangle&1)){unsigned swap=i[0];i[0]=i[1];i[1]=swap;}
    for(unsigned c=0;c<3;++c)
    {
        if(indices[i[c]]>=vertices) return false;
        output[c]=static_cast<unsigned short>(copy*vertices+indices[i[c]]);
    }
    return true;
}
inline bool SmallEnough(float pixelScale,float radius,float extent,const float modelView[4][4])
{
    if(!std::isfinite(pixelScale) || pixelScale<=0 || !std::isfinite(radius) || radius<=0 ||
       !std::isfinite(extent) || extent<radius) return false;
    // Scaling/shearing needs a different error bound. Keep original wheels there.
    for(unsigned a=0;a<3;++a)for(unsigned b=a;b<3;++b)
    {
        float dot=0;
        for(unsigned c=0;c<3;++c)dot+=modelView[a][c]*modelView[b][c];
        if(!std::isfinite(dot) || std::fabs(dot-(a==b?1.0f:0.0f))>.001f) return false;
    }
    const float nearDepth=std::fabs(modelView[3][2])-extent*1.01f;
    const float offAxis=std::fmax(std::fabs(modelView[3][0]),std::fabs(modelView[3][1]))+extent*1.01f;
    const float pixels=radius*1.01f*pixelScale*(1.0f+offAxis/nearDepth)/nearDepth;
    return std::isfinite(pixels) && nearDepth>0 && pixels<=3.0f;
}
inline bool PoseClose(const float root[4][4],const float centers[Copies][3],const float axes[Copies][3],
                      const float wheels[Copies][4][4])
{
    for(unsigned i=0;i<Copies;++i)
    {
        float center[3],axis[3];Transform(root,centers[i],center,true);Transform(root,axes[i],axis,false);
        float positionError=0,axisError=0;
        for(unsigned c=0;c<3;++c)
        {
            const float dp=center[c]-wheels[i][3][c],da=axis[c]-wheels[i][0][c];
            positionError+=dp*dp;axisError+=da*da;
        }
        // Keep live wheels for substantial suspension or steering changes.
        if(!std::isfinite(positionError) || !std::isfinite(axisError) || positionError>0.15f*0.15f || axisError>0.05f*0.05f) return false;
    }
    return true;
}
}
#endif
