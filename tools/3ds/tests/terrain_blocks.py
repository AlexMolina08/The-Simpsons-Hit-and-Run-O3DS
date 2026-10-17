#!/usr/bin/env python3
"""Run production terrain broad phase against its corrected linear reference."""
from pathlib import Path
import subprocess
import tempfile,sys
hierarchy="--hierarchy" in sys.argv
ROOT=Path(__file__).resolve().parents[3]
source=(ROOT/'code/render/DSG/IntersectDSG.cpp').read_text()
def extract(signature):
 start=source.index(signature);opening=source.index('{',start);depth=1;end=opening+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
body=extract('int IntersectDSG::mFlatTriFast')
prelude=r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#include "code/render/DSG/triblocks.h"
#define rAssert(x) assert(x)
#define rReleaseAssert(x) assert(x)
#define SRR2_OLD3DS_FLAT_TRI_FAST
namespace rmt {struct Vector {float x,y,z;};}
template<class T> struct Array {std::vector<T> values;int mUseSize=0;T& operator[](int i){return values.at(i);}bool IsSetUp(){return !values.empty();}};
static unsigned sTriangleBlockBytes;
struct IntersectDSG {
 Array<int> mTriIndices;Array<rmt::Vector> mTriPts,mTriNorms;Array<unsigned char> mTerrainType;
 bool msbInScratchPad=false;rmt::Vector* mspVertexData=nullptr;
 TriBlocks::Bounds* mTriangleBlocks=nullptr;unsigned mTriangleBlockBytes=0;
 enum {UNINIT_PT=0,X_GT_PT=1,X_LT_PT=2,Z_GT_PT=4,Z_LT_PT=8,INIT_PT=16,FOUND_PT=31};
 int Reference(int&,rmt::Vector*,rmt::Vector&);
 int mFlatTriFast(int&,rmt::Vector*,rmt::Vector&);
 void BuildTriangleBlocks();
};
'''
tests=r'''
int main(){
 std::mt19937 rng(983097);std::uniform_real_distribution<float> position(-100,100);
 unsigned comparisons=0;
 for(int trial=0;trial<2000;++trial){
  IntersectDSG d;unsigned n=1+rng()%2048;
  for(unsigned t=0;t<n;++t){
   float ox=(t/8)*10.0f,oz=(trial%7)*20.0f;
   for(int c=0;c<3;++c){d.mTriIndices.values.push_back(d.mTriPts.values.size());d.mTriPts.values.push_back({ox+position(rng),position(rng),oz+position(rng)});}
   d.mTriNorms.values.push_back({0,1,0});d.mTerrainType.values.push_back(t%255);
  }
  d.mTriIndices.mUseSize=d.mTriIndices.values.size();d.mTriPts.mUseSize=d.mTriPts.values.size();
  // Nonfinite geometry must leave whole blocks conservative.
  if(trial%29==0)d.mTriPts.values[rng()%d.mTriPts.values.size()].x=NAN;
  if(trial%31==0)d.mTriPts.values[rng()%d.mTriPts.values.size()].z=INFINITY;
  d.BuildTriangleBlocks();assert(sTriangleBlockBytes<=TriBlocks::Budget);
  unsigned firstBytes=d.mTriangleBlockBytes;d.BuildTriangleBlocks();assert(d.mTriangleBlockBytes==firstBytes);
  for(int query=0;query<100;++query){
   rmt::Vector a[3]={{position(rng)+float(n/16)*10,position(rng),position(rng)}},b[3];
   if(query%9==0){a[0]=d.mTriPts.values[rng()%d.mTriPts.values.size()];a[0].x+=query%2?.01f:-.01f;}
   if(query==1)a[0].x=NAN;
   if(query==2)a[0].z=INFINITY;
   memcpy(b,a,sizeof(a));rmt::Vector na={123,456,789},nb=na;
   int ia=rng()%n,ib=ia;
   for(;;){int ra=d.Reference(ia,a,na),rb=d.mFlatTriFast(ib,b,nb);
    assert(ra==rb&&ia==ib&&!memcmp(a,b,sizeof(a))&&!memcmp(&na,&nb,sizeof(na)));++comparisons;
    if(ra<0)break;
    --ia;--ib;if(ia<0)break;
    // Real caller restores the same query before asking for the next candidate.
    a[0]=b[0]={float(query),0,float(trial)};
   }
  }
  std::free(d.mTriangleBlocks);sTriangleBlockBytes-=d.mTriangleBlockBytes;
 }
 assert(sTriangleBlockBytes==0);
 // Budget pressure must fall back without allocating or retaining stale blocks.
 IntersectDSG d;d.mTriIndices.mUseSize=24;sTriangleBlockBytes=TriBlocks::Budget;
 d.BuildTriangleBlocks();assert(!d.mTriangleBlocks&&d.mTriangleBlockBytes==0);
 printf("%u production terrain comparisons: exact candidate order, terrain, vertices, normals, misses, NaN/Inf, rebuilds and budget fallback\n",comparisons);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-terrain-') as td:
 temp=Path(td);cpp=temp/'test.cpp'
 cpp.write_text(prelude+body.replace('IntersectDSG::mFlatTriFast','IntersectDSG::Reference')+'\n#define SRR2_OLD3DS_TRI_BLOCKS\n'+body+extract('void IntersectDSG::BuildTriangleBlocks()')+tests)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',*(['-DSRR2_OLD3DS_TRI_HIERARCHY'] if hierarchy else []),'-I',str(ROOT),str(cpp),'-o',str(temp/'test')],check=True)
 subprocess.run([str(temp/'test')],check=True)
