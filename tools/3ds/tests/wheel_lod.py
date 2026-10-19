#!/usr/bin/env python3
"""Verify wheel LOD topology, attribute retention and conservative pose gates."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
code=r'''
#include <pddi/pica/wheellod.hpp>
#include <cassert>
#include <cstdio>
#include <limits>
#include <vector>
using namespace picaWheelLOD;
int main(){
 unsigned rng=8712;auto word=[&](){rng=rng*1664525u+1013904223u;return rng;};unsigned triangles=0;
 for(unsigned test=0;test<12000;++test){
  unsigned vertices=3+word()%16381,indexCount=3+word()%600;bool strip=test&1;if(!strip)indexCount-=indexCount%3;
  std::vector<unsigned short> indices(indexCount);for(auto& x:indices)x=word()%vertices;
  for(unsigned copy=0;copy<Copies;++copy)for(unsigned t=0;t<TriangleCount(indexCount,strip);++t){
   unsigned short output[3];assert(Triangle(output,indices.data(),indexCount,vertices,copy,t,strip));
   unsigned a=strip?t:t*3,b=a+1,c=a+2;if(strip&&(t&1)){unsigned swap=a;a=b;b=swap;}
   assert(output[0]==copy*vertices+indices[a]&&output[1]==copy*vertices+indices[b]&&output[2]==copy*vertices+indices[c]);++triangles;
  }
 }
 float identity[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}},centers[4][3]={{-1,0,-1},{1,0,-1},{-1,0,1},{1,0,1}},axes[4][3]={{1,0,0},{1,0,0},{1,0,0},{1,0,0}},wheels[4][4][4];
 for(unsigned i=0;i<4;++i){memcpy(wheels[i],identity,64);memcpy(wheels[i][3],centers[i],12);}
 float view[4][4];memcpy(view,identity,64);view[3][2]=60;
 assert(SmallEnough(200,.5f,3,view));
 assert(!SmallEnough(800,.5f,3,view)); // Camera zoom keeps live wheels.
 view[3][0]=300;assert(!SmallEnough(200,.5f,3,view));view[3][0]=0;
 view[0][0]=2;assert(!SmallEnough(200,.5f,3,view));view[0][0]=1;
 view[3][2]=2;assert(!SmallEnough(200,.5f,3,view));view[3][2]=60;
 assert(!SmallEnough(0,.5f,3,view));
 assert(!SmallEnough(std::numeric_limits<float>::quiet_NaN(),.5f,3,view));
 assert(PoseClose(identity,centers,axes,wheels));
 wheels[1][3][1]=.151f;assert(!PoseClose(identity,centers,axes,wheels));wheels[1][3][1]=0;
 wheels[2][0][2]=.051f;assert(!PoseClose(identity,centers,axes,wheels));wheels[2][0][2]=0;
 wheels[3][3][0]=std::numeric_limits<float>::quiet_NaN();assert(!PoseClose(identity,centers,axes,wheels));
 assert(!WantFar(false,54.99f*54.99f)&&WantFar(false,55*55)&&WantFar(true,45*45)&&!WantFar(true,44.99f*44.99f));
 assert(!WantFar(true,std::numeric_limits<float>::infinity()));
 for(unsigned i=0;i<10000;++i){Vertex a,b;for(unsigned c=0;c<3;++c){a.position[c]=float(int(word()%8192)-4096)/1024;a.normal[c]=float(int(word()%1024)-512)/1024;}a.uv[0]=.5;a.uv[1]=.25;for(auto& x:a.colour)x=word();assert(BakeVertex(b,a,identity));assert(!memcmp(a.uv,b.uv,8)&&!memcmp(a.colour,b.colour,4));for(unsigned c=0;c<3;++c){assert(a.position[c]==b.position[c]);assert(a.normal[c]==b.normal[c]);}}
 Vertex a={},b={};a.position[0]=std::numeric_limits<float>::infinity();assert(!BakeVertex(b,a,identity));
 puts("PASS: wheel LOD topology and provoking corners, UV/color retention, nonfinite fallback, 55/45m hysteresis, suspension/steering guards");printf("triangles=%u\n",triangles);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-wheel-lod-') as td:
 d=Path(td);(d/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(root/'libs/pure3d'),str(d/'test.cpp'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
