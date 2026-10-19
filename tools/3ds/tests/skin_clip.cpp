#include <pddi/pica/skinclip.hpp>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <limits>
static uint32_t state=923;
static float random(float lo,float hi){state^=state<<13;state^=state>>17;state^=state<<5;return lo+(hi-lo)*float(state&0xffffff)/16777216;}
static float f24(float x,bool trunc){if(x==0)return x;int e;double m=std::frexp(double(x),&e)*131072;return float(std::ldexp(trunc?std::trunc(m):std::round(m),e-17));}
int main(){unsigned checked=0;
 for(unsigned test=0;test<20000;++test){
  picaSkin::Source vertices[32]={};float matrices[25][4][4]={};
  const unsigned bones=1+test%25;
  for(unsigned b=0;b<bones;++b){for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)matrices[b][r][c]=random(-3,3);for(unsigned a=0;a<3;++a)matrices[b][3][a]=random(-2000,2000);matrices[b][3][3]=1;}
  for(auto& v:vertices){
   for(unsigned a=0;a<3;++a)v.position[a]=random(-3.9f,3.9f);
   float sum=0;for(unsigned j=0;j<3;++j){v.weights[j]=random(0,(1-sum)*.5f);sum+=v.weights[j];}
   if(test%3==0)v.weights[0]=1;
   for(unsigned j=0;j<4;++j)v.indices[j]=unsigned(random(0,float(bones)))%bones;
  }
  auto bounds=picaSkinClip::Build(vertices,32,bones);assert(bounds.valid);float low[3],high[3];assert(picaSkinClip::Transform(bounds,matrices,bones,low,high));
  auto merged=picaSkinClip::Build(vertices,16,bones);auto tail=picaSkinClip::Build(vertices+16,16,bones);assert(picaSkinClip::Merge(merged,tail));
  float mergedLow[3],mergedHigh[3];assert(picaSkinClip::Transform(merged,matrices,bones,mergedLow,mergedHigh));
  for(unsigned a=0;a<3;++a)assert(mergedLow[a]==low[a]&&mergedHigh[a]==high[a]);
  for(const auto& v:vertices)for(unsigned mode=0;mode<3;++mode){
   float weights[4]={v.weights[0],v.weights[1],v.weights[2],1.f-(v.weights[2]+v.weights[1]+v.weights[0])};
   if(weights[0]==1)weights[1]=weights[2]=weights[3]=0;
   for(unsigned a=0;a<3;++a){float out=0;
    for(unsigned j=0;j<4;++j){if(v.weights[0]==1&&j)break;const auto& m=matrices[v.indices[j]];float p=m[3][a];
     if(mode){float sum=0;for(unsigned r=0;r<4;++r){float position=r==3?1:float(picaSkin::Quantize(v.position[r],8192))/8192;sum=f24(sum+f24(f24(m[r][a],mode==2)*f24(position,mode==2),mode==2),mode==2);}p=sum;out=f24(out+f24(p*f24(weights[j],mode==2),mode==2),mode==2);}
     else {p=m[0][a]*v.position[0]+m[1][a]*v.position[1]+m[2][a]*v.position[2]+m[3][a];out+=p*weights[j];}
    }
    assert(out>=low[a]&&out<=high[a]);++checked;
   }
  }
 }
 picaSkin::Source v={};v.weights[0]=1;v.indices[0]=0;
 auto b=picaSkinClip::Build(&v,1,1);assert(b.valid);
 v.weights[0]=.5;v.weights[1]=.5;v.indices[3]=255;assert(!picaSkinClip::Build(&v,1,1).valid);v.indices[3]=0;v.weights[1]=0;
 v.weights[0]=-.1;assert(!picaSkinClip::Build(&v,1,1).valid);v.weights[0]=1;
 v.position[0]=std::numeric_limits<float>::quiet_NaN();assert(!picaSkinClip::Build(&v,1,1).valid);
 float matrices[25][4][4]={},lo[3],hi[3];matrices[0][0][0]=matrices[0][1][1]=matrices[0][2][2]=matrices[0][3][3]=1;
 assert(picaSkinClip::Transform(b,matrices,1,lo,hi));matrices[0][0][0]=std::numeric_limits<float>::infinity();assert(!picaSkinClip::Transform(b,matrices,1,lo,hi));
 printf("PASS: %u CPU/packed float24 coordinate comparisons; nonfinite and negative-weight inputs retain normal rendering\n",checked);
}
