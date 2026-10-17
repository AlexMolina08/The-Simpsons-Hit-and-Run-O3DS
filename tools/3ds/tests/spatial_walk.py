#!/usr/bin/env python3
"""Compare production sphere traversal order and marks on random binary trees."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
source=(ROOT/'code/render/Culling/SpatialTreeIter.cpp').read_text()
def extract(signature):
 start=source.index(signature);opening=source.index('{',start);depth=1;end=opening+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
body=extract('void SpatialTreeIter::MarkAll( int')
prelude=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
using tMark=int;
struct AAPlane3f {unsigned axis;float pos;};
struct SphereSP {float xyz[3],radius;float CompareTo(AAPlane3f& p){float delta=p.pos-xyz[p.axis];return std::abs(delta)<=radius?0:delta;}};
struct BoxPts {void CutOffGT(AAPlane3f&){}void CutOffLT(AAPlane3f&){} };
struct SpatialNode {AAPlane3f mSubDivPlane;};
template<class T> struct ContiguousBinNode {T mData;int size,right;int GetSubTreeSize(){return size;}int LChildOffset(){return 1;}int RChildOffset(){return right;}};
struct List {std::vector<SpatialNode*> nodes;void Add(SpatialNode* p){nodes.push_back(p);}};
struct SpatialTreeIter {
 ContiguousBinNode<SpatialNode>* mpRootNode;List mCurNodes;std::vector<int> mNodeMarks;
 void MarkAll(int,BoxPts&,SphereSP&,tMark);
 void Fast(int,BoxPts&,SphereSP&,tMark);
 void MarkSpherePlanes(int,SphereSP&,tMark);
};
std::mt19937 rng(34083);
unsigned Build(std::vector<ContiguousBinNode<SpatialNode>>& nodes,int depth){
 unsigned id=nodes.size();nodes.push_back({{{rng()%3,float(int(rng()%200)-100)}},0,0});
 if(depth>0&&rng()%4){Build(nodes,depth-1);unsigned right=nodes.size();Build(nodes,depth-1);nodes[id].size=nodes.size()-id-1;nodes[id].right=right-id;}
 return id;
}
int main(){
 unsigned queries=0;
 for(int tree=0;tree<300;++tree){
  std::vector<ContiguousBinNode<SpatialNode>> nodes;Build(nodes,10);
  for(int query=0;query<300;++query){
   SpatialTreeIter a{nodes.data(),{},std::vector<int>(nodes.size(),17)},b=a;
   SphereSP sphere{{float(int(rng()%300)-150),float(int(rng()%300)-150),float(int(rng()%300)-150)},float(rng()%80)};
   if(query==0)sphere.xyz[0]=NAN;
   BoxPts box;a.MarkAll(0,box,sphere,2);b.Fast(0,box,sphere,2);
   assert(a.mCurNodes.nodes==b.mCurNodes.nodes&&a.mNodeMarks==b.mNodeMarks);++queries;
  }
 }
 printf("%u production sphere traversals: exact node order and accumulated marks\n",queries);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-spatial-') as td:
 p=Path(td);cpp=p/'test.cpp'
 # Rewrite only recursive reference names in the optimized wrapper; production
 # helper keeps its original name and body.
 cpp.write_text(prelude+body+'\n#define SRR2_OLD3DS_SPATIAL_WALK_FAST\n'+body.replace('SpatialTreeIter::MarkAll','SpatialTreeIter::Fast')+extract('void SpatialTreeIter::MarkSpherePlanes'))
 subprocess.run(['clang++','-std=c++14','-O2','-fsanitize=address,undefined','-I',str(ROOT),str(cpp),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
