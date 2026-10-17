#!/usr/bin/env python3
"""Compare production ground-plane preparation with identical ordered physics inputs."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'code/worldsim/worldphysicsmanager.cpp').read_text()
def extract(signature):
 start=s.index(signature);end=s.index('{',start)+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
body=extract('void WorldPhysicsManager::UpdateSimluatingDynamicObjectGroundPlanes()')
code=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
namespace rmt {struct Vector {float x=0,y=0,z=0;Vector()=default;Vector(float a,float b,float c):x(a),y(b),z(c){};};}
namespace sim {enum {simSimulationCtrl};}
namespace PhysicsAIRef {enum {redBrickVehicle,PlayerCharacter,Other};}
struct State {int control=0,mAIRefIndex=2;int GetControl(){return control;}};
struct Object {rmt::Vector position;State state;int index=0;
 State* GetSimState(){return &state;}rmt::Vector rPosition(){return position;}int GetGroundPlaneIndex(){return index;}};
struct Event {int index;rmt::Vector point,normal;};
struct Pool {std::vector<Event> events;void UpdateGroundPlane(int i,rmt::Vector& p,rmt::Vector& n){events.push_back({i,p,n});}};
struct Intersector {unsigned calls=0;void FindIntersection(rmt::Vector& p,bool& found,rmt::Vector& n,rmt::Vector& v){
 ++calls;found=std::isfinite(p.x)&&std::isfinite(p.z)&&p.y>=0;
 if(found){n={p.x*.25f,1,p.z*.5f};v={p.x,10,p.z};}}} intersect;
Intersector* GetIntersectManager(){return &intersect;}
struct Slot {Object* mDynamicPhysDSG=nullptr;};
struct WorldPhysicsManager {int mNumCollisionAreas=24,mMaxDynamics=20;bool mCollisionAreaAllocated[24]={};
 Slot mCurrentDynamics[24][20];Pool pool;Pool* mGroundPlanePool=&pool;
 void UpdateSimluatingDynamicObjectGroundPlanes();void Reference();};
'''+body.replace('WorldPhysicsManager::UpdateSimluatingDynamicObjectGroundPlanes','WorldPhysicsManager::Reference')+'\n#define SRR2_OLD3DS_GROUND_REUSE\n'+body+r'''
int main(){std::mt19937 rng(724);unsigned referenceQueries=0,optimizedQueries=0;
 for(int trial=0;trial<2000;++trial){WorldPhysicsManager w;Object objects[96];
  for(unsigned i=0;i<96;++i){auto& o=objects[i];o.index=i%80;o.position={float(rng()%50),float(rng()%4)-1,float(rng()%40)};
   if(i%19==0)o.position.x=-0.0f;if(i%23==0)o.position.x=0.0f;
   if(i%29==0)o.position.z=NAN;if(i%31==0)o.position.z=INFINITY;
   o.state.control=rng()%3;o.state.mAIRefIndex=rng()%3;}
  for(int a=0;a<24;++a){w.mCollisionAreaAllocated[a]=rng()%4!=0;
   for(int b=0;b<20;++b)if(rng()%5)w.mCurrentDynamics[a][b].mDynamicPhysDSG=&objects[rng()%96];}
  intersect.calls=0;w.Reference();referenceQueries+=intersect.calls;auto expected=w.pool.events;w.pool.events.clear();
  intersect.calls=0;w.UpdateSimluatingDynamicObjectGroundPlanes();optimizedQueries+=intersect.calls;
  assert(expected.size()==w.pool.events.size());
  for(unsigned i=0;i<expected.size();++i){auto& a=expected[i];auto& b=w.pool.events[i];assert(a.index==b.index);
   assert(!memcmp(&a.point,&b.point,sizeof(a.point))&&!memcmp(&a.normal,&b.normal,sizeof(a.normal)));}
  // A new pass must observe changed positions and terrain results.
  for(auto& o:objects)o.position.x+=1.25f;
  w.pool.events.clear();w.Reference();expected=w.pool.events;w.pool.events.clear();w.UpdateSimluatingDynamicObjectGroundPlanes();
  assert(expected.size()==w.pool.events.size());for(unsigned i=0;i<expected.size();++i){auto& a=expected[i];auto& b=w.pool.events[i];
   assert(a.index==b.index&&!memcmp(&a.point,&b.point,sizeof(a.point))&&!memcmp(&a.normal,&b.normal,sizeof(a.normal)));}
 }
 assert(optimizedQueries<referenceQueries);
 printf("PASS: 2,000 production ground-plane passes, ordered writes bit-exact, misses, NaN/Inf, signed zero, slot aliases and fresh passes; queries %u reference / %u reused\n",referenceQueries,optimizedQueries);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-ground-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-Wno-unused-variable','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
