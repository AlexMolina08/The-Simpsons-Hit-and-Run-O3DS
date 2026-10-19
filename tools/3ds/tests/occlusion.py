#!/usr/bin/env python3
"""Falsify conservative cell coverage with independent perturbed triangles."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=r'''
#include <pddi/pica/occlusion.hpp>
#include <cassert>
#include <cstdio>
#include <limits>
using namespace picaOcclusion;
unsigned state=123974;unsigned next(){state=state*1664525u+1013904223u;return state;}
float unit(){return float(next()>>8)/16777216.f;}
float edge(float ax,float ay,float bx,float by,float x,float y){return (bx-ax)*(y-ay)-(by-ay)*(x-ax);}
bool inside(float v[3][2],float x,float y){float a=edge(v[0][0],v[0][1],v[1][0],v[1][1],x,y),b=edge(v[1][0],v[1][1],v[2][0],v[2][1],x,y),c=edge(v[2][0],v[2][1],v[0][0],v[0][1],x,y);return (a>=0 && b>=0 && c>=0)||(a<=0 && b<=0 && c<=0);}
int main(){
 unsigned covered=0,checks=0;
 for(unsigned trial=0;trial<6000;++trial){
  Grid grid;Point p[3];
  for(auto& v:p)v={2+unit()*(Width-4),2+unit()*(Height-4),.05f+unit()*.4f,.05f+unit()*.4f,.2f,.3f,true};
  grid.Add(p[0],p[1],p[2],0);assert(grid.visits<=CellBudget);
  for(unsigned sample=0;sample<64;++sample){unsigned i=next()%Cells;if(grid.stamp[i]!=grid.generation)continue;++covered;
   for(unsigned perturb=0;perturb<24;++perturb){float v[3][2];for(unsigned j=0;j<3;++j){v[j][0]=p[j].x+(unit()*2-1)*p[j].ex;v[j][1]=p[j].y+(unit()*2-1)*p[j].ey;}
    for(unsigned corner=0;corner<4;++corner){float x=float(i%Width)+(corner&1),y=float(i/Width)+(corner>>1);assert(inside(v,x,y));++checks;}
   }
  }
  grid.Clear();for(unsigned i=0;i<Cells;++i)assert(grid.stamp[i]!=grid.generation);
 }
 assert(covered>1000);
 Grid grid;Point a={2,2,.01f,.01f,.2f,.3f,true},b={46,2,.01f,.01f,.2f,.3f,true},c={24,78,.01f,.01f,.2f,.3f,true};
 grid.Add(a,b,c,1);assert(grid.triangles==0);grid.Add(a,b,c,2);assert(grid.triangles==1);
 Point hidden={24,30,1,1,.7f,.8f,true};assert(grid.Hidden(hidden));hidden.nearDepth=.1f;assert(!grid.Hidden(hidden));
 hidden.nearDepth=.3f;assert(!grid.Hidden(hidden));hidden.nearDepth=.7f;hidden.x=1;assert(!grid.Hidden(hidden));
 grid.Clear(true);for(unsigned i=0;i<300;++i)grid.Add(a,b,c,0);assert(grid.visits==CellBudget && grid.triangles<=256);
 grid.generation=65535;grid.Clear();assert(grid.generation==1);for(unsigned i=0;i<Cells;++i)assert(grid.stamp[i]==0);
 Transform transform;float identity[4][4]={};for(unsigned i=0;i<4;++i)identity[i][i]=1;
 transform.Prepare(identity,identity);float center[3]={0,0,-.5f},extent[3]={.1f,.1f,.01f};auto result=transform.Project(center,extent,.001f);assert(result.valid && result.nearDepth<.49f && result.farDepth>.51f);
 center[0]=std::numeric_limits<float>::quiet_NaN();assert(!transform.Project(center,extent,.001f).valid);
 identity[3][3]=0;transform.Prepare(identity,identity);assert(!transform.valid);
 printf("PASS: %u uncertain triangles, %u covered cells, %u independent corner checks; winding, holes, depth, reset and fixed work budget. Not hardware timing.\n",6000u,covered,checks);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-occlusion-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
