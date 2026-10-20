#pragma STDC FENV_ACCESS ON
#include <p3d/effects/particlebounds.hpp>
#include <pddi/pica/clipbounds.hpp>
#include <cassert>
#include <cfenv>
#include <cstdio>
#include <cstdint>
#include <initializer_list>
static unsigned rng=231171;
static float random(float low,float high){rng=rng*1664525u+1013904223u;return low+(high-low)*float(rng&0xffffff)/16777216.f;}
static float f24(float x,bool truncate){if(x==0)return x;int e;double m=std::frexp(double(x),&e)*131072;return float(std::ldexp(truncate?std::trunc(m):std::round(m),e-17));}
static float dot(const float row[4],const float* v,bool truncate){float sum=0;for(unsigned i=0;i<4;++i)sum=f24(sum+f24(f24(row[i],truncate)*f24(v[i],truncate),truncate),truncate);return sum;}
static void transform(const float m[4][4],const float*p,float*out){for(unsigned a=0;a<3;++a)out[a]=m[0][a]*p[0]+m[1][a]*p[1]+m[2][a]*p[2]+m[3][a];}
int main(){unsigned tested=0,rejected=0,corners=0,depth=0;
 for(int rounding:{FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){
  assert(!fesetround(rounding));
  for(unsigned scene=0;scene<25000;++scene){
   float m[4][4]={{}},identity[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
   for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)m[r][c]=random(-2,2);
   m[3][3]=1;for(unsigned a=0;a<3;++a)m[3][a]=random(-100,100);
   float center[3]={random(-10000,10000),random(-10000,10000),random(-10000,10000)};
   float current[16][3],previous[16][3],size[16],angle[16];p3dParticleBounds::Bounds bounds;bounds.Begin();
   for(unsigned i=0;i<16;++i){for(unsigned a=0;a<3;++a){current[i][a]=center[a]+random(-100,100);previous[i][a]=current[i][a]+random(-1,1);}size[i]=random(-4,4);angle[i]=random(-32,32);bounds.Add(current[i],previous[i],size[i],angle[i]);}
   assert(bounds.valid && bounds.count==16);float low[3],high[3];assert(p3dParticleBounds::Transform(m,bounds.low,bounds.high,low,high));
   bool finite=p3dParticleBounds::FiniteDepth(m,bounds);depth+=finite;
   for(unsigned i=0;i<16;++i){float p[3],old[3];transform(m,current[i],p);transform(m,previous[i],old);for(unsigned a=0;a<3;++a)assert(low[a]<=p[a]&&high[a]>=p[a]);if(finite)assert(std::fabs(p[2])>=.001f&&std::fabs(old[2])>=.001f);}
   const float extent=2*bounds.size+.001f;for(unsigned a=0;a<2;++a){low[a]-=extent;high[a]+=extent;}
   float projection[4][4]={{random(.2,3),0,0,0},{0,random(.2,3),0,0},{0,0,1.001f,-.1f},{0,0,1,0}};
   auto clip=picaClipBounds::Prepare(identity,projection);unsigned plane=picaClipBounds::Reject(clip,low,high);++tested;if(!plane)continue;++rejected;
   for(unsigned i=0;i<16;++i){float p[3];transform(m,current[i],p);float x=cosf(angle[i])*size[i],y=sinf(angle[i])*size[i];
    float v[4][4]={{p[0]+x,p[1]+y,p[2],1},{p[0]-x,p[1]-y,p[2],1},{p[0]-y,p[1]+x,p[2],1},{p[0]+y,p[1]-x,p[2],1}};
    for(unsigned mode=0;mode<2;++mode)for(unsigned c=0;c<4;++c){float q[4];for(unsigned a=0;a<4;++a)q[a]=dot(projection[a],v[c],mode);float distance=q[3]+((plane-1)%2?-q[(plane-1)/2]:q[(plane-1)/2]);assert(distance<0);++corners;}
   }
  }
 }
 p3dParticleBounds::Bounds b;b.Begin();float p[3]={0,0,1};b.Add(p,p,1,0);assert(b.valid);p[0]=NAN;b.Add(p,p,1,0);assert(!b.valid);
 assert(rejected>50000&&depth>50000);printf("PASS: %u scenes / %u emitter rejections / %u actual-corner float24 checks; affine enclosures and finite-depth proofs across four rounding modes.\n",tested,rejected,corners);
}
