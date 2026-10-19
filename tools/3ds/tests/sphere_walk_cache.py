#!/usr/bin/env python3
"""Compare production cached sphere traversal, marks and tree replacement."""
from pathlib import Path
import subprocess,tempfile,sys
regions="--regions" in sys.argv
root=Path(__file__).resolve().parents[3]
cpp=(root/'code/render/Culling/SpatialTreeIter.cpp').read_text()
header=(root/'code/render/Culling/SpatialTreeIter.h').read_text()
sphere=(root/'code/render/Culling/SphereSP.cpp').read_text()
sphere_header=(root/'code/render/Culling/SphereSP.h').read_text()
def extract(source, signature):
 a=source.index(signature);b=source.index('{',a);e=b+1;depth=1
 while depth:
  depth+=(source[e]=='{')-(source[e]=='}');e+=1
 return source[a:e]
# Locate the live body by its current upper-bound predicate.
a=sphere.index('    if( irPlane.mPosn > mCenter');start=sphere.rfind('float SphereSP::CompareTo(',0,a);compare=extract(sphere[start:],'float SphereSP::CompareTo(')
prelude=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>
#include <cfenv>
#include <render/Culling/spherewalkcache.hpp>
using tMark=int;
struct AAPlane3f {int mAxis;float mPosn;};
struct Vec {float xyz[3];float operator[](int i)const{return xyz[i];}};
struct SphereSP {
 Vec mCenter;float mRadius;
 static float msIntersectionEpsilon;
 float CompareTo(AAPlane3f&);
'''+extract(sphere_header,'   void PlaneBounds(')+r'''
};
float SphereSP::msIntersectionEpsilon=0.01f;
struct Bounds {};
struct BoxPts {void SetTo(Bounds){}};
struct SpatialNode {AAPlane3f mSubDivPlane;int liveObject;};
template<class T> struct ContiguousBinNode {
 T mData;int size,right;
 int GetSubTreeSize(){return size;}
 int LChildOffset(){return 1;}
 int RChildOffset(){return right;}
};
struct List {
 std::vector<SpatialNode*> nodes;
 void Add(SpatialNode* node){nodes.push_back(node);}
 void ClearUse(){nodes.clear();}
 void Allocate(int n){nodes.clear();nodes.reserve(n);}
};
struct Marks {
 std::vector<int> values;
 void Allocate(int n){values.assign(n,0);}
 int& operator[](int n){return values[n];}
};
struct SpatialTree {
 std::vector<ContiguousBinNode<SpatialNode>> nodes;
 auto GetRoot()->ContiguousBinNode<SpatialNode>* {return nodes.data();}
 Bounds GetBounds(){return {};}
};
struct SpatialTreeIter {
 ContiguousBinNode<SpatialNode>* mpRootNode=nullptr;
 List mCurNodes,mCurAlwaysVisNodes;
 List* mpCurNodeList=nullptr;
 Marks mNodeMarks;BoxPts mBBox;
 Old3dsSphereWalkCache mSphereWalkCache;
 void SetToRoot(SpatialTree&);
 void Clear();
 void MarkSpherePlanes(int,SphereSP&,tMark);
 void MarkSphereQuery(SphereSP&,tMark);
 void MarkSphereBounds(int,const float[6],tMark,Old3dsSphereWalkCache::Entry*);
};
std::mt19937 randomState(331927);
void Build(SpatialTree& tree,int depth,bool full=false){
 unsigned id=tree.nodes.size();
 tree.nodes.push_back({{{int(randomState()%3),float(int(randomState()%200)-100)},int(randomState())},0,0});
 if(depth>0 && (full || randomState()%4)){
  Build(tree,depth-1,full);unsigned right=tree.nodes.size();Build(tree,depth-1,full);
  tree.nodes[id].size=tree.nodes.size()-id-1;tree.nodes[id].right=right-id;
 }
}
int main(){
 static_assert(sizeof(Old3dsSphereWalkCache)<=6144,"Bounded node-index scratch per walker");
 unsigned queries=0,replays=0,oversized=0;
 for(unsigned t=0;t<180;++t){
  SpatialTree tree;Build(tree,t==0 ? 16 : 11,t<2);
  SpatialTreeIter a,b;a.SetToRoot(tree);b.SetToRoot(tree);
  SphereSP queriesPool[16];
  for(auto& q:queriesPool)q={{{float(int(randomState()%300)-150),float(int(randomState()%300)-150),float(int(randomState()%300)-150)}},float(randomState()%90)};
  for(unsigned q=0;q<300;++q){
   SphereSP current=queriesPool[q%16];
   if(q%37==0)current.mRadius=-current.mRadius;
   if(q%43==0)current.mRadius=-0.0f;
   if(q%101==0)current.mRadius=std::numeric_limits<float>::quiet_NaN();
   if(q%107==0)current.mCenter.xyz[0]=std::numeric_limits<float>::infinity();
   if(q%109==0)current.mCenter.xyz[1]=std::numeric_limits<float>::max();
   if(q%31==0)SphereSP::msIntersectionEpsilon=(q%62 ? 0.02f : 0.01f);
   if(q==150){
    // Rebuild planes in the same allocation, then exercise actual Clear/SetToRoot.
    a.Clear();b.Clear();
    for(auto& node:tree.nodes)node.mData.mSubDivPlane.mPosn+=3.25f;
    a.SetToRoot(tree);b.SetToRoot(tree);
   }
   tree.nodes[randomState()%tree.nodes.size()].mData.liveObject=int(randomState());
   for(unsigned repeat=0;repeat<3;++repeat){
    int mark=1<<((q+repeat)%12);
    a.mCurNodes.ClearUse();b.mCurNodes.ClearUse();
    if(repeat==2 && q%41==0)std::fesetround(FE_DOWNWARD);
    a.MarkSpherePlanes(0,current,mark);b.MarkSphereQuery(current,mark);
    std::fesetround(FE_TONEAREST);
    assert(a.mCurNodes.nodes==b.mCurNodes.nodes);
    assert(a.mNodeMarks.values==b.mNodeMarks.values);
    if(a.mCurNodes.nodes.size()>64)++oversized;
    if(repeat==0){
     float bounds[6];uint32_t key[6];current.PlaneBounds(bounds);
     if(Old3dsSphereWalkCache::MakeKey(bounds,key) && b.mSphereWalkCache.Find(key))++replays;
    }
    ++queries;
   }
  }
 }

#ifdef SRR2_OLD3DS_SPHERE_REGION_REUSE
 // Moving bounds cross exact plane thresholds, including strict and inclusive
 // endpoints. Planes use adjacent floats, signed zeros and negative radii.
 for(unsigned t=0;t<50;++t){
  SpatialTree tree;Build(tree,8);
  SpatialTreeIter a,b;a.SetToRoot(tree);b.SetToRoot(tree);
  for(unsigned q=0;q<400;++q){
   float x=float(int(q)-200)*0.003f;
   SphereSP current={{{x,1.0f+x,2.0f-x}},q%3 ? 2.0f : -0.005f};
   if(q%5==0){
    const auto& plane=tree.nodes[q%tree.nodes.size()].mData.mSubDivPlane;
    current.mCenter.xyz[plane.mAxis]=plane.mPosn;
    current.mRadius=q%10 ? -0.01f : 0.01f;
   }
   a.mCurNodes.ClearUse();b.mCurNodes.ClearUse();
   a.MarkSpherePlanes(0,current,1);b.MarkSphereQuery(current,1);
   assert(a.mCurNodes.nodes==b.mCurNodes.nodes);
   assert(a.mNodeMarks.values==b.mNodeMarks.values);++queries;
  }
 }
 // Exhaustively exercise each recorded decision's boundary and immediate neighbors.
 for(float plane: {-100.0f,-0.0f,0.0f,0.01f,100.0f}) {
  float values[]={-200.0f,std::nextafter(plane,-INFINITY),plane,std::nextafter(plane,INFINITY),200.0f};
  for(unsigned axis=0;axis<3;++axis)for(float low:values)for(float high:values){
   Old3dsSphereWalkCache cache;float initial[6]={0,0,0,0,0,0};initial[axis]=low;initial[axis+3]=high;
   uint32_t key[6];assert(Old3dsSphereWalkCache::MakeKey(initial,key));
   auto* record=cache.Begin(key);Old3dsSphereWalkCache::Record(record,0);
   float side=plane>high ? 1.0f : plane<low ? -1.0f : 0.0f;
   cache.RecordPlane(axis,plane,side);cache.Commit(record);
   assert(cache.FindRegion(initial));
   for(float nextLow:values)for(float nextHigh:values){
    float next[6]={0,0,0,0,0,0};next[axis]=nextLow;next[axis+3]=nextHigh;
    float expected=plane>nextHigh ? 1.0f : plane<nextLow ? -1.0f : 0.0f;
    assert(bool(cache.FindRegion(next))==(side==expected));++queries;
   }
  }
 }
 assert(Old3dsSphereWalkCache::Metrics().regionHits>1000);
 printf("Region replays: %u; bytes per walker: %zu\n",Old3dsSphereWalkCache::Metrics().regionHits,sizeof(Old3dsSphereWalkCache));
#endif
 assert(replays && oversized);
 printf("PASS: %u production sphere traversals; exact node order and accumulated marks; %u replayable queries; %u oversized fallbacks; live contents, epsilon changes, alternate rounding, nonfinite bounds and same-address tree rebuilds\n",queries,replays,oversized);
}
'''
methods='\n'.join(extract(cpp,s) for s in ['void SpatialTreeIter::MarkSpherePlanes(', 'void SpatialTreeIter::MarkSphereQuery(', 'void SpatialTreeIter::MarkSphereBounds('])
methods+='\n'+extract(header,'void SpatialTreeIter::SetToRoot(')+'\n'+extract(header,'void SpatialTreeIter::Clear()')
with tempfile.TemporaryDirectory(prefix='srr-sphere-cache-') as td:
 p=Path(td);(p/'test.cpp').write_text('#define SRR2_OLD3DS_SPHERE_WALK_CACHE\n'+('#define SRR2_OLD3DS_SPHERE_REGION_REUSE\n' if regions else '')+prelude+'\n'+compare+'\n'+methods)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
