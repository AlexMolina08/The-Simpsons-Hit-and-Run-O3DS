#!/usr/bin/env python3
"""Compare production registration and removal while preserving exact pair order."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/sim/simcollision/collisionmanager.cpp').read_text()
def extract(signature):
 a=source.index(signature);b=source.index('{',a);e=b+1;depth=1
 while depth:
  depth+=(source[e]=='{')-(source[e]=='}');e+=1
 return source[a:e]
methods='\n'.join(extract(s) for s in ['void CollisionManager::Area::AddCollisionObject(', 'bool CollisionManager::Area::RemoveCollisionObject(', 'void CollisionManager::Area::RemovePair(', 'void CollisionManager::Area::AddPair('])
prelude=r'''
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <utility>
#define rAssert(x) ((void)0)
static unsigned long long containsCalls;
static bool swapRemoval;
static std::vector<std::pair<int,int>> events;
struct CollisionObject {
 int id;bool isStatic=false,autoPair=true,collideStatic=true;
 bool IsStatic(){return isStatic;}
 bool GetAutoPair(){return autoPair;}
 bool CollideWithStatic(){return collideStatic;}
 void AddRef(){events.push_back({1,id});}
 void Release(){events.push_back({2,id});}
};
struct Pair {
 CollisionObject* mObjectA=nullptr;CollisionObject* mObjectB=nullptr;
 bool Contains(CollisionObject* p){++containsCalls;return mObjectA==p || mObjectB==p;}
 void Set(CollisionObject* a,CollisionObject* b){mObjectA=a;mObjectB=b;}
};
template<class T>struct Array {
 std::vector<T> data;
 int GetSize(){return int(data.size());}
 T& operator[](int i){return data.at(i);}
 T& NewElem(){data.emplace_back();return data.back();}
 void Add(T value){data.push_back(value);}
 bool Contains(T value){return std::find(data.begin(),data.end(),value)!=data.end();}
 bool Remove(T value){auto found=std::find(data.begin(),data.end(),value);if(found==data.end())return false;RemoveAt(found-data.begin());return true;}
 void RemoveAt(int i){if(swapRemoval){data[i]=data.back();data.pop_back();}else data.erase(data.begin()+i);}
};
'''
controller=r'''
struct CollisionManager {
 struct Area {
  bool mUseExclusiveAutoPair=false;
  Array<CollisionObject*> mObjectList;
  Array<Pair> mPairList;
  void AddCollisionObject(CollisionObject*);
  bool RemoveCollisionObject(CollisionObject*);
  void RemovePair(CollisionObject*,CollisionObject*);
  void AddPair(CollisionObject*,CollisionObject*);
 };
};
'''
main=r'''
static uint32_t randomState=0x20261017;
unsigned Next(){randomState^=randomState<<13;randomState^=randomState>>17;return randomState^=randomState<<5;}
int main(){
 unsigned operations=0;unsigned long long referenceTests=0,optimizedTests=0;
 for(unsigned run=0;run<160;++run){
  swapRemoval=run%2;CollisionObject objects[24];for(unsigned i=0;i<24;++i)objects[i].id=i;
  Reference::CollisionManager::Area a;Optimized::CollisionManager::Area b;
  for(unsigned step=0;step<1200;++step){
   auto* x=&objects[Next()%24];auto* y=&objects[Next()%24];unsigned op=Next()%5;
   a.mUseExclusiveAutoPair=b.mUseExclusiveAutoPair=(Next()%3==0);
   if(step%31==0){x->isStatic=Next()%2;x->autoPair=Next()%2;x->collideStatic=Next()%2;}
   bool add=(op==0 || op==4) && !a.mObjectList.Contains(x);
   bool pair=a.mObjectList.Contains(x) && a.mObjectList.Contains(y) && x!=y;
   auto execute=[&](auto& area){
    bool removed=false;
    if(add)area.AddCollisionObject(x);
    if(op==1)removed=area.RemoveCollisionObject(x);
    if((op==2 || op==4) && pair)area.AddPair(x,y);
    if(op==3)area.RemovePair(x,y);
    // Probe the automatically appended tail in both orientations.
    if(add && area.mPairList.GetSize()){
     auto tail=area.mPairList[area.mPairList.GetSize()-1];
     area.AddPair(tail.mObjectA,tail.mObjectB);area.AddPair(tail.mObjectB,tail.mObjectA);
    }
    return removed;
   };
   events.clear();containsCalls=0;bool ar=execute(a);auto expected=events;referenceTests+=containsCalls;
   events.clear();containsCalls=0;bool br=execute(b);optimizedTests+=containsCalls;
   assert(ar==br && events==expected);
   assert(a.mObjectList.data==b.mObjectList.data);
   assert(a.mPairList.GetSize()==b.mPairList.GetSize());
   for(int i=0;i<a.mPairList.GetSize();++i){assert(a.mPairList[i].mObjectA==b.mPairList[i].mObjectA);assert(a.mPairList[i].mObjectB==b.mPairList[i].mObjectB);}
   ++operations;
  }
 }
 assert(Optimized::old3dsPairTailHits && optimizedTests<referenceTests);
 printf("PASS: %u production collision registration/removal operations; exact objects, references and pair order; Contains calls %llu reference / %llu optimized; tail hits %u/%u; static/dynamic flags, reversed pairs and stable/swap removals\n",operations,referenceTests,optimizedTests,Optimized::old3dsPairTailHits,Optimized::old3dsPairTailChecks);
}
'''
code=prelude+'namespace Reference {\n'+controller+methods+'\n}\n#define SRR2_OLD_3DS\n#define SRR2_OLD3DS_COLL_PREP\nnamespace Optimized {static unsigned old3dsPairTailChecks=0,old3dsPairTailHits=0;\n'+controller+methods+'\n}\n'+main
with tempfile.TemporaryDirectory(prefix='srr-pair-tail-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
