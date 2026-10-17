#!/usr/bin/env python3
"""Exercise production detector stamps across areas, batch boundaries and camera calls."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/sim/simcollision/collisiondetector.cpp').read_text()
def extract(sig):
 a=s.index(sig);b=s.index('{',a);e=b+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
prelude=r'''
#include <cassert>
#include <cstdio>
#include <vector>
#include <cstdint>
#define SRR2_OLD_3DS
#define SRR2_OLD3DS_COLLVOL_CACHE
using u64=uint64_t;const unsigned SYSCLOCK_ARM11=1000;
u64 svcGetSystemTick(){static u64 t=0;return ++t;}
unsigned g_old3dsUpdateAllRedundant=0,g_old3dsUpdateAllCalls=0,g_old3dsUpdateAllRedundantSamples=0,g_old3dsUpdateAllSamples=0,g_old3dsDetectPasses=0,g_old3dsPairUpdates=0,g_old3dsUpdateAllStaticSkip=0;
double g_old3dsUpdateAllRedundantMs=0,g_old3dsUpdateAllMs=0;
#define rAssert(x) assert(x)
struct CollisionVolume {unsigned stamp=0,updates=0;int transform=1,position=0;bool updated=false,prepared=false,sphere=false;
 unsigned UpdatePassStamp(){return stamp;}void SetUpdatePassStamp(unsigned s){stamp=s;}
 void UpdateAll(){++updates;position=transform;updated=!sphere;}
 void CapturePreparedUpdated(){prepared=updated;}void RestorePreparedUpdated(){updated=prepared;}
};
struct Object {CollisionVolume volume;bool fixed=false;bool IsStatic(){return fixed;}};
struct CollisionObjectPair {Object*mObjectA,*mObjectB;bool mNeedsUpdate=true;unsigned mCollide=0;CollisionVolume*VolumeA(){return &mObjectA->volume;}CollisionVolume*VolumeB(){return &mObjectB->volume;}};
struct Collision{};
template<class T>struct TArray {std::vector<T> data;int GetSize(){return data.size();}T&GetAt(int i){return data.at(i);}};
struct CollisionDetector {
 unsigned mPreparationBatchStamp=0,mNewColl=0;float mDt=0,mCollisionDistance=0,mMinCollisionDistance=0;
 TArray<Collision>* list=nullptr;std::vector<int> trace;
 void SetCollisionList(TArray<Collision>&l){list=&l;}TArray<Collision>*GetCollisionList(){return list;}
 void DetectCollision(CollisionVolume*a,CollisionVolume*b){trace.push_back(a->position*100+b->position);mNewColl=1;}
 void DetectCollision(TArray<CollisionObjectPair>*,TArray<Collision>*,float,const bool);
 unsigned BeginPreparationBatch();void EndPreparationBatch(unsigned p){mPreparationBatchStamp=p;}
};
'''
methods='\n'.join(extract(sig) for sig in ['static unsigned NextUpdatePassStamp()', 'unsigned CollisionDetector::BeginPreparationBatch()', 'static void UpdateVolumeForPass(', 'void CollisionDetector::DetectCollision(TArray<CollisionObjectPair>*'])
tests=r'''
int main(){
 Object a,b,stationary;stationary.fixed=true;stationary.volume.position=7;b.volume.sphere=true;
 TArray<CollisionObjectPair> area;area.data={{&a,&b},{&a,&stationary},{&a,&b}};TArray<Collision> contacts;CollisionDetector d;
 for(int batch=0;batch<2000;++batch){
  a.volume.transform=batch%11;b.volume.transform=batch%13;
  unsigned start=a.volume.updates,prev=d.BeginPreparationBatch();
  for(unsigned i=0;i<5;++i){a.volume.updated=b.volume.updated=false;d.DetectCollision(&area,&contacts,0.01f,false);assert(a.volume.updated&&!b.volume.updated);}
  d.EndPreparationBatch(prev);
  assert(a.volume.updates==start+1&&b.volume.updates==start+1);
  // Camera/solver mutation between single-area calls must force fresh preparation.
  a.volume.transform=99;d.DetectCollision(&area,&contacts,0.01f,false);assert(a.volume.position==99&&a.volume.updates==start+2);
  b.volume.transform=88;d.DetectCollision(&area,&contacts,0.01f,false);assert(b.volume.position==88&&b.volume.updates==start+3);
 }
 assert(stationary.volume.updates==0);assert(d.mPreparationBatchStamp==0);
 for(auto&p:area.data)assert(p.mCollide==1);
 puts("PASS: 2,000 multi-area batches; one update per object/batch, exact Updated state, fresh camera/solver boundaries, static exclusion and ordered pair execution");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-collision-batch-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+methods+tests)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Wno-unused-parameter','-fsanitize=address,undefined','-DSRR2_OLD3DS_COLL_PREP',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
