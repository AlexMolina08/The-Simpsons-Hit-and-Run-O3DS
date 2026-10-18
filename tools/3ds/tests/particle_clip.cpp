#pragma STDC FENV_ACCESS ON
#include <pddi/pica/particleclip.hpp>
#include <cfenv>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <limits>
#include <initializer_list>
static uint32_t randomState=79;
static float randomFloat(float lo,float hi){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return lo+(hi-lo)*float(randomState&0xffffff)/16777216.0f;}
static float f24(float x,bool truncate){if(x==0)return x;int e;double m=std::frexp(double(x),&e)*131072;return float(std::ldexp(truncate?std::trunc(m):std::round(m),e-17));}
static float dot(const float row[4],const float v[4],bool truncate){float sum=0;for(unsigned i=0;i<4;++i)sum=f24(sum+f24(f24(row[i],truncate)*f24(v[i],truncate),truncate),truncate);return sum;}

int main(){unsigned rejected=0,points=0;
 for(int rounding:{FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){
  assert(!fesetround(rounding));
  for(unsigned n=0;n<25000;++n){
   const float mv[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
   float projection[4][4]={{0}};
   projection[0][1]=randomFloat(.2,3);projection[1][0]=randomFloat(-3,-.2);
   projection[2][2]=1.001f;projection[2][3]=-.1f;projection[3][2]=1;
   auto transform=picaClipBounds::Prepare(mv,projection);assert(transform.valid);
   float center[3]={randomFloat(-2048,2048),randomFloat(-2048,2048),randomFloat(-20,400)};
   float size=randomFloat(-32,32),angle=randomFloat(-10000,10000);
   unsigned plane=picaParticleClip::Reject(transform,center,size,angle);if(!plane)continue;++rejected;
   float x=cosf(angle)*size,y=sinf(angle)*size;
   float v[4][4]={{center[0]+x,center[1]+y,center[2],1},{center[0]-x,center[1]-y,center[2],1},
     {center[0]-y,center[1]+x,center[2],1},{center[0]+y,center[1]-x,center[2],1}};
   for(unsigned mode=0;mode<2;++mode)for(unsigned corner=0;corner<4;++corner){
    float clip[4];for(unsigned r=0;r<4;++r)clip[r]=dot(projection[r],v[corner],mode);
    const float distance=clip[3]+((plane-1)%2?-clip[(plane-1)/2]:clip[(plane-1)/2]);
    assert(distance<0);++points;
   }
  }
 }
 float m[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
 auto t=picaClipBounds::Prepare(m,m);float c[3]={1.00001f,0,0};assert(!picaParticleClip::Reject(t,c,0));
 assert(!picaParticleClip::Reject(t,c,std::numeric_limits<float>::infinity()));
 c[0]=std::numeric_limits<float>::quiet_NaN();assert(!picaParticleClip::Reject(t,c,1));
 c[0]=100;assert(!picaParticleClip::Reject(t,c,1,std::numeric_limits<float>::quiet_NaN()));assert(!picaParticleClip::Reject(t,c,500001));
 assert(rejected>50000);printf("PASS: %u conservative sprite rejections and %u actual-corner float24 checks across four rounding modes; grazing/nonfinite/oversized sprites stay visible.\n",rejected,points);
}
