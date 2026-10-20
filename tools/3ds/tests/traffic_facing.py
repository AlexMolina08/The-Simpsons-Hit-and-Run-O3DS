#!/usr/bin/env python3
"""Compare production facing-only reads with full original lane evaluation."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/roads/roadsegment.cpp').read_text()
original=subprocess.check_output(['git','show','HEAD:code/roads/roadsegment.cpp'],cwd=root,text=True)
def extract(s,signature):
 a=s.index(signature);b=s.index('{',a);end=b+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]
prelude=r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <random>
#include <limits>
#include <cfenv>
#include <cstdio>
#define LERP(f,a,b) ((1.0f-(f))*a+(f)*b)
namespace rmt {struct Vector{float x,y,z;Vector operator-(const Vector& b)const{return {x-b.x,y-b.y,z-b.z};}Vector operator+(const Vector& b)const{return {x+b.x,y+b.y,z+b.z};}Vector operator*(float s)const{return {x*s,y*s,z*s};}};}
struct RoadSegment {
 rmt::Vector normals[4],corners[4];unsigned reads=0,lanes=2;
 void GetEdgeNormal(int i,rmt::Vector& v){v=normals[i];}
 void GetCorner(int i,rmt::Vector& v){++reads;v=corners[i];}
 unsigned GetNumLanes(){return lanes;}
 void GetLaneLocation(float,int,rmt::Vector&,rmt::Vector&);
 void GetLaneFacing(float,rmt::Vector&);
};
'''
code=prelude
# Rename only the original member; production implementations remain verbatim.
code=code.replace(' void GetLaneFacing(float,rmt::Vector&);',' void GetLaneFacing(float,rmt::Vector&);\n void Original(float,int,rmt::Vector&,rmt::Vector&);')
code+=extract(original,'void RoadSegment::GetLaneLocation(').replace('RoadSegment::GetLaneLocation','RoadSegment::Original')
code+=extract(source,'void RoadSegment::GetLaneFacing(')+extract(source,'void RoadSegment::GetLaneLocation(')
code+=r'''
int main(){
 std::mt19937 rng(941);unsigned checked=0;
 for(int mode:{FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){
  assert(!fesetround(mode));
  for(unsigned n=0;n<4000;++n){
   RoadSegment road;road.lanes=1+rng()%4;
   for(auto& v:road.normals){v={float(int(rng()%100000)-50000)*.0001f,float(int(rng()%100000)-50000)*.0001f,float(int(rng()%100000)-50000)*.0001f};}
   for(auto& v:road.corners)v={float(rng()%1000),float(rng()%1000),float(rng()%1000)};
   if(n%13==0)road.normals[0].x=-0.0f;
   if(n%17==0)road.normals[2].y=std::numeric_limits<float>::infinity();
   if(n%19==0)road.normals[0].z=std::numeric_limits<float>::quiet_NaN();
   for(float t:{0.0f,1.0f,.13f,.5f,.87f})for(unsigned lane=0;lane<road.lanes;++lane){
    rmt::Vector posA,faceA,posB,faceB,only;
    road.Original(t,lane,posA,faceA);road.GetLaneLocation(t,lane,posB,faceB);
    assert(!memcmp(&posA,&posB,sizeof(posA)) && !memcmp(&faceA,&faceB,sizeof(faceA)));
    const auto before=road.reads;road.GetLaneFacing(t,only);
    assert(road.reads==before && !memcmp(&faceA,&only,sizeof(only)));++checked;
   }
  }
 }
 printf("PASS: %u production lane comparisons; four rounding modes, endpoints/interpolation, lane widths, signed zero/nonfinite normals; zero corner traversal.\n",checked);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-traffic-facing-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-frounding-math','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
