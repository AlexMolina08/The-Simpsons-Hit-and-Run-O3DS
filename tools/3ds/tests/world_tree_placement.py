#!/usr/bin/env python3
"""Execute actual world placement paths, including exhausted spatial-node arrays."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3];source=(root/'code/render/Culling/WorldScene.cpp').read_text()
a=source.index('void WorldScene::Place( IntersectDSG*');b=source.index('void WorldScene::Place(',a+10);ground=source[a:b]
a=source.index('void WorldScene::Place(    StaticEntityDSG*');b=source.index('void WorldScene::Place(',a+10);static=source[a:b]
code='''#include <cassert>
static unsigned staticFailures=0,groundFailures=0,assertions=0;
namespace WorldResidency {void PlacementFailure(bool ground){++(ground?groundFailures:staticFailures);}}
namespace Old3dsLandmark {unsigned rootPlaced=0,rootFallback=0;}
#define rTuneAssert(value) do {if(!(value))++assertions;} while(0)
namespace rmt {struct Vector{void SetTo(const Vector&){}void Add(const Vector&){}void Sub(const Vector&){} };struct Box3D{Vector low,high;};}
struct ISpatialProxyAA{};struct BoxPts:ISpatialProxyAA{struct {rmt::Vector mMin,mMax;}mBounds;};
struct SpatialNode;
struct IntersectDSG {SpatialNode* mpSpatialNode=nullptr;void GetBoundingBox(rmt::Box3D*){}};
struct StaticEntityDSG {bool mOld3dsLandmark=false;SpatialNode* mpSpatialNode=nullptr;void GetBoundingBox(rmt::Box3D*){}};
template<class T> struct Slots {bool accept=true;unsigned calls=0;bool Add(T&){++calls;return accept;}};
struct GroundSlots {int mUseSize=0,mSize=1;void Add(IntersectDSG*&){assert(mUseSize<mSize);++mUseSize;}};
struct SpatialNode {GroundSlots mIntersectElems;Slots<StaticEntityDSG*> mSEntityElems;};
struct Walker {SpatialNode root,target;SpatialNode& rSeekNode(ISpatialProxyAA&){return target;}SpatialNode& rIthNode(int){return root;}};
struct WorldScene {Walker mStaticTreeWalker;rmt::Vector mEpsilonOffset;void Place(IntersectDSG*);void Place(StaticEntityDSG*);};
'''+ground+static+'''
int main(){
 WorldScene scene;IntersectDSG ground;StaticEntityDSG entity;
 scene.Place(&ground);assert(ground.mpSpatialNode==&scene.mStaticTreeWalker.target);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
 scene.Place(&ground);assert(groundFailures==1 && scene.mStaticTreeWalker.target.mIntersectElems.mUseSize==1);
#endif
 scene.Place(&entity);assert(entity.mpSpatialNode==&scene.mStaticTreeWalker.target);
 scene.mStaticTreeWalker.target.mSEntityElems.accept=false;scene.Place(&entity);assert(entity.mpSpatialNode==&scene.mStaticTreeWalker.root);
 assert(!staticFailures);
 scene.mStaticTreeWalker.root.mSEntityElems.accept=false;scene.Place(&entity);
#ifndef SRR2_OLD3DS_NATIVE_BENCH
 assert(assertions==1);
#endif
#ifdef SRR2_OLD3DS_NATIVE_BENCH
 assert(staticFailures==1);
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='world-placement-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 for diagnostic in (False,True):
  subprocess.run(['clang++','-std=c++17','-DSRR2_OLD3DS_VISUAL_UPGRADE',*(['-DSRR2_OLD3DS_NATIVE_BENCH'] if diagnostic else []),'-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
print('PASS: actual ground/static placement, successful root fallback and reported exhausted-node failures; release behavior preserved.')
