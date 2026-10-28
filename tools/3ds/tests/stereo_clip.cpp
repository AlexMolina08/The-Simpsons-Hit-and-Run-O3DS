#include <pddi/pica/clipbounds.hpp>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

static uint32_t randomState=371;
static float randomFloat(float low,float high)
{
    randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;
    return low+(high-low)*float(randomState&0xffffff)/16777216.0f;
}
static float f24(float value,bool truncate)
{
    if(value==0)return value;
    int exponent;
    double mantissa=std::frexp(double(value),&exponent)*131072;
    return float(std::ldexp(truncate?std::trunc(mantissa):std::round(mantissa),exponent-17));
}
static float dot(const float row[4],const float point[4],bool truncate)
{
    float sum=0;
    for(unsigned i=0;i<4;++i)
        sum=f24(sum+f24(f24(row[i],truncate)*f24(point[i],truncate),truncate),truncate);
    return sum;
}
int main()
{
    unsigned rejected=0,comparisons=0;
    for(unsigned test=0;test<30000;++test)
    {
        float model[4][4]={{0}},projection[4][4]={{0}},wide[4][4],low[3],high[3];
        for(unsigned row=0;row<3;++row)
        {
            for(unsigned column=0;column<3;++column)model[row][column]=randomFloat(-1,1);
            model[row][3]=randomFloat(-512,512);
            low[row]=randomFloat(-128,128);high[row]=low[row]+randomFloat(0,32);
        }
        model[3][3]=1;
        const float scale=randomFloat(.1f,10);
        projection[0][1]=randomFloat(.2f,3)*scale;
        projection[1][0]=randomFloat(-3,-.2f)*scale;
        projection[2][2]=scale;projection[2][3]=-.1f*scale;projection[3][2]=scale;
        const float shift=randomFloat(0,.03f),convergence=randomFloat(.01f,2000);
        memcpy(wide,projection,sizeof(wide));
        assert(picaClipBounds::WidenStereo(wide[3],shift,convergence));
        const auto bounds=picaClipBounds::Prepare(model,wide);
        assert(bounds.valid);
        if(!picaClipBounds::Reject(bounds,low,high))continue;
        ++rejected;
        for(unsigned rounding=0;rounding<2;++rounding)
            for(unsigned sample=0;sample<16;++sample)
        {
            float point[4]={0,0,0,1},eye[4],clip[4];
            for(unsigned axis=0;axis<3;++axis)
                point[axis]=sample<8?((sample&(1u<<axis))?low[axis]:high[axis]):randomFloat(low[axis],high[axis]);
            for(unsigned row=0;row<4;++row)eye[row]=dot(model[row],point,rounding);
            for(unsigned row=0;row<4;++row)clip[row]=dot(projection[row],eye,rounding);
            for(int direction=-1;direction<=1;direction+=2)
            {
                const float displacement=f24(f24(clip[3]*f24(direction*shift,rounding),rounding)-f24(direction*shift*convergence,rounding),rounding);
                const float horizontal=f24(clip[1]-displacement,rounding);
                const bool visible=clip[3]>=0 && std::fabs(clip[0])<=clip[3] && std::fabs(horizontal)<=clip[3];
                assert(!visible);++comparisons;
            }
        }
    }
    // Either-eye edge points and W=0 crossings must survive conservative culling.
    const float identity[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    float projection[4][4];memcpy(projection,identity,sizeof(projection));
    assert(picaClipBounds::WidenStereo(projection[3],.03f,30));
    const auto bounds=picaClipBounds::Prepare(identity,projection);
    const float point[3]={0,1.8f,0};assert(!picaClipBounds::Reject(bounds,point,point));
    const float invalid=std::numeric_limits<float>::quiet_NaN();
    assert(!picaClipBounds::WidenStereo(projection[3],invalid,1));
    assert(!picaClipBounds::WidenStereo(projection[3],.03f,-1));
    assert(!picaClipBounds::WidenStereo(projection[3],.2f,1));
    assert(rejected>10000);
    std::printf("PASS: %u stereo bounds rejections, %u two-eye float24 checks; scaled projections, edge points and invalid fallback\n",rejected,comparisons);
}
