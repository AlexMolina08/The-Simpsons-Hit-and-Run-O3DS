#!/usr/bin/env python3
"""Compare production area callbacks, including reference and collision events."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'code/worldsim/worldphysicsmanager.cpp').read_text()
def extract(name):
 a=s.index('void WorldPhysicsManager::'+name+'(');b=s.index('{',a);e=b+1;depth=1
 while depth:
  depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
methods='\n'.join(extract(n) for n in ['SubmitStaticsPseudoCallback','SubmitAnimCollisionsPseudoCallback','SubmitAnimCollisionsForUpdateOnly'])
prelude=r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <array>
#include <cstring>
#define rAssert(x) ((void)0)
#define rDebugPrintf(...) ((void)0)
#define BEGIN_PROFILE(x) ((void)0);
#define END_PROFILE(x) ((void)0);
static unsigned long long indexedComparisons;
#include "slotindex.hpp"
using Event=std::array<int,3>;
static std::vector<Event> events;
namespace rmt { struct Vector { float x,y,z; }; }
namespace sim {
struct CollisionObject {int id;void SetAutoPair(bool v){events.push_back({0,id,v});}};
struct SimState {CollisionObject object;CollisionObject* GetCollisionObject(){return &object;}};
}
struct StaticPhysDSG {
 sim::SimState state;
 void AddRef(){events.push_back({1,state.object.id,0});}
 void Release(){events.push_back({2,state.object.id,0});}
 sim::SimState* mpSimState(){return &state;}
};
struct AnimCollisionEntityDSG {
 sim::SimState state;
 void AddRef(){events.push_back({3,state.object.id,0});}
 void Release(){events.push_back({4,state.object.id,0});}
 sim::SimState* GetSimState(){return &state;}
};
struct CollisionManager {
 void AddCollisionObject(sim::CollisionObject* object,int area){events.push_back({5,object->id,area});}
 void AddPair(sim::CollisionObject* a,sim::CollisionObject* b,int area){events.push_back({6,a->id,b->id+1000*area});}
 void RemoveCollisionObject(sim::CollisionObject* object,int area){events.push_back({7,object->id,area});}
};
static std::vector<StaticPhysDSG*> staticQuery;
static std::vector<AnimCollisionEntityDSG*> animatedQuery;
template<class T> struct ReserveArray {int mUseSize=0;std::vector<T> data;T operator[](int i){return data[i];}};
struct IntersectManager {
 void FindStaticPhysElems(rmt::Vector&,float,ReserveArray<StaticPhysDSG*>& r){r.data=staticQuery;r.mUseSize=r.data.size();}
 void FindAnimPhysElems(rmt::Vector&,float,ReserveArray<AnimCollisionEntityDSG*>& r){r.data=animatedQuery;r.mUseSize=r.data.size();}
};
static IntersectManager intersect;
IntersectManager* GetIntersectManager(){return &intersect;}
struct StaticRecord {StaticPhysDSG* mStaticPhysDSG=nullptr;bool clean=false;};
struct AnimatedRecord {AnimCollisionEntityDSG* mAnimCollisionEntityDSG=nullptr;bool clean=false;};
'''
controller=r'''
struct WorldPhysicsManager {
 static const int INVALID_COLLISION_AREA=-1;
 int mMaxStatics=30,mMaxAnimCollisions=20,mMaxUpdateAnimCollisions=64;
 StaticRecord mCurrentStatics[1][80];
 AnimatedRecord mCurrentAnimCollisions[1][80],mCurrentUpdateAnimCollisions[1][80];
 CollisionManager* mCollisionManager;
 void SubmitStaticsPseudoCallback(rmt::Vector&,float,int,sim::SimState*,bool);
 void SubmitAnimCollisionsPseudoCallback(rmt::Vector&,float,int,sim::SimState*);
 void SubmitAnimCollisionsForUpdateOnly(rmt::Vector&,float,int);
};
'''
main=r'''
static uint32_t randomState=0x31415926;
static unsigned Next(){randomState^=randomState<<13;randomState^=randomState>>17;return randomState^=randomState<<5;}
int main(){
 static_assert(sizeof(Old3dsSlotIndex<void*>) <= 1160,"Bounded local pointer index");
 StaticPhysDSG statics[150];AnimCollisionEntityDSG animated[150];
 for(int i=0;i<150;++i){statics[i].state.object.id=i;animated[i].state.object.id=i+200;}
 CollisionManager collision;
 Reference::WorldPhysicsManager a;Optimized::WorldPhysicsManager b;
 a.mCollisionManager=b.mCollisionManager=&collision;
 rmt::Vector position={};sim::SimState caller;caller.object.id=900;
 unsigned calls=0;
 for(unsigned test=0;test<5000;++test){
  a.mMaxStatics=b.mMaxStatics=1+Next()%80;
  a.mMaxAnimCollisions=b.mMaxAnimCollisions=1+Next()%80;
  a.mMaxUpdateAnimCollisions=b.mMaxUpdateAnimCollisions=1+Next()%80;
  staticQuery.clear();animatedQuery.clear();
  for(unsigned i=0,n=Next()%100;i<n;++i)staticQuery.push_back(Next()%19 ? &statics[Next()%150] : nullptr);
  for(unsigned i=0,n=Next()%100;i<n;++i)animatedQuery.push_back(Next()%19 ? &animated[Next()%150] : nullptr);
  auto compare=[&](unsigned mode){
   events.clear();
   if(mode==0)a.SubmitStaticsPseudoCallback(position,10,0,&caller,true);
   if(mode==1)a.SubmitAnimCollisionsPseudoCallback(position,10,0,&caller);
   if(mode==2)a.SubmitAnimCollisionsForUpdateOnly(position,10,0);
   auto expected=events;events.clear();
   if(mode==0)b.SubmitStaticsPseudoCallback(position,10,0,&caller,true);
   if(mode==1)b.SubmitAnimCollisionsPseudoCallback(position,10,0,&caller);
   if(mode==2)b.SubmitAnimCollisionsForUpdateOnly(position,10,0);
   assert(events==expected);++calls;
   for(unsigned slot=0;slot<80;++slot){
    assert(a.mCurrentStatics[0][slot].mStaticPhysDSG==b.mCurrentStatics[0][slot].mStaticPhysDSG);
    assert(a.mCurrentStatics[0][slot].clean==b.mCurrentStatics[0][slot].clean);
    assert(a.mCurrentAnimCollisions[0][slot].mAnimCollisionEntityDSG==b.mCurrentAnimCollisions[0][slot].mAnimCollisionEntityDSG);
    assert(a.mCurrentAnimCollisions[0][slot].clean==b.mCurrentAnimCollisions[0][slot].clean);
    assert(a.mCurrentUpdateAnimCollisions[0][slot].mAnimCollisionEntityDSG==b.mCurrentUpdateAnimCollisions[0][slot].mAnimCollisionEntityDSG);
    assert(a.mCurrentUpdateAnimCollisions[0][slot].clean==b.mCurrentUpdateAnimCollisions[0][slot].clean);
   }
  };
  compare(0);compare(1);compare(2);
 }
 printf("PASS: %u production area callbacks; identical membership, clean flags, reference events, collision additions/removals and pair order; sparse/full lists, duplicate/null results, caps and fallback\n",calls);
}
'''
code=prelude+'namespace Reference {\n'+controller+methods+'\n}\n#define SRR2_OLD3DS_AREA_MEMBERSHIP\nnamespace Optimized {\n'+controller+methods+'\n}\n'+main
with tempfile.TemporaryDirectory(prefix='srr-area-membership-') as td:
 p=Path(td);(p/'test.cpp').write_text(code);(p/'slotindex.hpp').write_text((root/'code/worldsim/old3dsslotindex.hpp').read_text())
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-private-field','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
