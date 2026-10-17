#include <pddi/pica/clipbounds.hpp>
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
 for(unsigned test=0;test<100000;++test){
  float mv[4][4]={{0}},projection[4][4]={{0}},low[3],high[3];
  for(unsigned r=0;r<3;++r){for(unsigned c=0;c<3;++c)mv[r][c]=randomFloat(-1,1);mv[r][3]=randomFloat(-2048,2048);low[r]=randomFloat(-2048,2048);high[r]=low[r]+randomFloat(0,128);}
  mv[3][3]=1;projection[0][1]=randomFloat(.2,3);projection[1][0]=randomFloat(-3,-.2);projection[2][2]=1.001f;projection[2][3]=-.1f;projection[3][2]=1;
  auto transform=picaClipBounds::Prepare(mv,projection);assert(transform.valid);
  unsigned plane=picaClipBounds::Reject(transform,low,high);if(!plane)continue;++rejected;
  for(unsigned mode=0;mode<2;++mode)for(unsigned sample=0;sample<16;++sample){
   float source[4]={0,0,0,1},eye[4],clip[4];
   for(unsigned j=0;j<3;++j)source[j]=sample<8?((sample&(1u<<j))?low[j]:high[j]):randomFloat(low[j],high[j]);
   for(unsigned j=0;j<4;++j)eye[j]=dot(mv[j],source,mode);
   for(unsigned j=0;j<4;++j)clip[j]=dot(projection[j],eye,mode);
   float distance=clip[3]+((plane-1)%2?-clip[(plane-1)/2]:clip[(plane-1)/2]);
   assert(distance<0);++points;
  }
 }
 assert(rejected>10000);
 float mv[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}},p[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
 auto t=picaClipBounds::Prepare(mv,p);float low[3]={-1,-1,-1},high[3]={1,1,1};assert(!picaClipBounds::Reject(t,low,high));
 for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){float old=mv[i][j];mv[i][j]=std::numeric_limits<float>::quiet_NaN();assert(!picaClipBounds::Prepare(mv,p).valid);mv[i][j]=old;}
 low[0]=std::numeric_limits<float>::infinity();assert(!picaClipBounds::Reject(t,low,high));low[0]=2;assert(!picaClipBounds::Reject(t,low,high));
 // Grazing all side planes remains visible through the full safety band.
 for(unsigned axis=0;axis<2;++axis)for(int sign:{-1,1})for(unsigned i=0;i<1000;++i){
  float lo[3]={0,0,0},hi[3]={0,0,0};lo[axis]=hi[axis]=sign*(1+float(i)/1000000);assert(!picaClipBounds::Reject(t,lo,hi));
 }
 std::printf("PASS: %u conservative rejections, %u float24 corner/interior comparisons; nonfinite, invalid and grazing bounds stay visible\n",rejected,points);
}
