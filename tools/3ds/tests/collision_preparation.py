#!/usr/bin/env python3
"""Compare production traversal, bounds inputs and candidate ordering in both modes."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
volume=(root/'libs/sim/simcollision/collisionvolume.cpp').read_text()
detector=(root/'libs/sim/simcollision/collisiondetector.cpp').read_text()
def extract(source,sig):
 a=source.index(sig);b=source.index('{',a);end=b+1;depth=1
 while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[a:end]
prelude=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <random>
#include <cstdint>
#define rAssert(x) assert(x)
using std::fabs;
float Fabs(float f){return fabs(f);}float Sqr(float f){return f*f;}
float Max(float a,float b){return a>b?a:b;}float Min(float a,float b){return a<b?a:b;}
struct Vector {float x=0,y=0,z=0;
 void Add(const Vector&a,const Vector&b){x=a.x+b.x;y=a.y+b.y;z=a.z+b.z;}
 void Sub(const Vector&a,const Vector&b){x=a.x-b.x;y=a.y-b.y;z=a.z-b.z;}
 void Scale(float f){x*=f;y*=f;z*=f;}
};
struct Matrix {float m[4][3];
 void RotateVector(const Vector&a,Vector*b)const{Vector v;v.x=a.x*m[0][0]+a.y*m[1][0]+a.z*m[2][0];v.y=a.x*m[0][1]+a.y*m[1][1]+a.z*m[2][1];v.z=a.x*m[0][2]+a.y*m[1][2]+a.z*m[2][2];*b=v;}
 void Transform(const Vector&a,Vector*b)const{RotateVector(a,b);b->x+=m[3][0];b->y+=m[3][1];b->z+=m[3][2];}
};
namespace rmt {using ::Vector;}
template<class T>struct TList {std::vector<T> v;int GetSize(){return v.size();}T GetAt(int i){return v.at(i);}T operator[](int i){return v.at(i);}};
enum {CollisionVolumeType,SphereVolumeType,CylinderVolumeType,OBBoxVolumeType,WallVolumeType,BBoxVolumeType,VolAxisNotOriented=0};
struct CollisionVolume;
struct CollisionObject {bool manual=false,flexible=false;Matrix transforms[4];CollisionVolume* root=nullptr;
 bool IsManualUpdate(){return manual;}
 const Matrix& GetTransform(int i) {static Matrix scratch; if(flexible){scratch=transforms[i];return scratch;}return transforms[i];}
 CollisionVolume* GetCollisionVolume(){return root;}
};
struct CollisionVolume {
 Vector mPosition,mBoxSize,mDP;float mSphereRadius=1;int mType=SphereVolumeType,mObjRefIndex=0;
 CollisionObject* mCollisionObject=nullptr;TList<CollisionVolume*>* mSubVolumeList=nullptr;
 bool mUpdated=false,mVisible=true;
 virtual ~CollisionVolume(){};virtual void SetRotation(){};virtual void UpdateBBox(){};
 void UpdateAll();void UpdatePos();void UpdateRot();void UpdatePosRot();
 int Type(){return mType;}bool IsVisible(){return mVisible;}TList<CollisionVolume*>* SubVolumeList(){return mSubVolumeList;}
};
struct CylinderVolume:CollisionVolume {Vector mAxisRef,mAxis;float mLength=1,mCylinderRadius=1;bool mFlatEnd=false;void SetRotation();void UpdateBBox();};
struct OBBoxVolume:CollisionVolume {Vector mAxisRef[3],mAxis[3];float mLength[3]={1,2,3};int mAxisOrientation=0;void SetRotation();void UpdateBBox();};
struct WallVolume:CollisionVolume {Vector mNormalRef,mNormal;void SetRotation();};
struct BBoxVolume:CollisionVolume {void UpdateBBox();void AddBox(Vector&,Vector&,CollisionVolume*)const;};
struct CollisionDetector {float mCollisionDistance=0.01f;std::vector<unsigned> visits;
 void DetectOnSubVolume(CollisionVolume*,CollisionVolume*);
 void DetectCollision(CollisionVolume*,CollisionVolume*);
 void DetectCollisionOverlapping(CollisionVolume*,CollisionVolume*);
 void DetectColl(CollisionVolume*a,CollisionVolume*b){visits.push_back(a->mObjRefIndex*1000+b->mObjRefIndex);}
};
'''
methods='\n'.join(extract(volume,s) for s in ['void CollisionVolume::UpdateAll()', 'void CollisionVolume::UpdatePosRot()', 'void CollisionVolume::UpdatePos()', 'void CollisionVolume::UpdateRot()', 'void CylinderVolume::SetRotation()', 'inline float ApproxSqrt(', 'void CylinderVolume::UpdateBBox()', 'void OBBoxVolume::SetRotation()', 'void OBBoxVolume::UpdateBBox()', 'void WallVolume::SetRotation()', 'void BBoxVolume::UpdateBBox()', 'void BBoxVolume::AddBox('])
methods+='\n'+extract(detector,'static bool SubIsBBInForWall(')+'\n'+extract(detector,'static inline bool IsBBIn(')+'\n'+extract(detector,'void CollisionDetector::DetectOnSubVolume(')
methods=methods.replace('static inline bool IsBBIn(', 'static inline bool IsBBInRaw(')
pos=methods.index('void CollisionDetector::DetectOnSubVolume(')
methods=methods[:pos]+'unsigned boundsTests=0;\nstatic bool IsBBIn(CollisionVolume*a,CollisionVolume*b,float p){++boundsTests;return IsBBInRaw(a,b,p); }\n'+methods[pos:]
methods+='\n'+extract(detector,'void CollisionDetector::DetectCollision(CollisionVolume* volA, CollisionVolume* volB)')+'\n'+extract(detector,'void CollisionDetector::DetectCollisionOverlapping(')
tests=r'''
uint64_t hash=1469598103934665603ull;
void fold(const void* p,unsigned n){auto*b=(const unsigned char*)p;for(unsigned i=0;i<n;++i){hash^=b[i];hash*=1099511628211ull;}}
int main(){
 std::mt19937 random(991);
 auto f=[&](){return float(int(random()%200)-100)/31.0f;};
 for(unsigned run=0;run<20000;++run){
  CollisionObject obj;obj.manual=run%7==0;obj.flexible=run%3==0;
  for(auto& m:obj.transforms)for(auto& r:m.m)for(auto& v:r)v=f();
  BBoxVolume root,nested;CylinderVolume cylinder;OBBoxVolume box;WallVolume wall;CollisionVolume sphere;
  root.mType=nested.mType=BBoxVolumeType;cylinder.mType=CylinderVolumeType;box.mType=OBBoxVolumeType;wall.mType=WallVolumeType;
  TList<CollisionVolume*> children,grandchildren;children.v={&cylinder,&nested,&wall};grandchildren.v={&box,&sphere};
  root.mSubVolumeList=&children;nested.mSubVolumeList=&grandchildren;obj.root=&root;
  CollisionVolume* nodes[]={&root,&nested,&cylinder,&box,&wall,&sphere};
  for(auto* n:nodes){n->mCollisionObject=run%11?&obj:nullptr;n->mObjRefIndex=random()%4;n->mPosition={f(),f(),f()};n->mDP={f(),f(),f()};n->mBoxSize={1,2,3};}
  cylinder.mAxisRef={f(),f(),f()};cylinder.mFlatEnd=run%2;wall.mNormalRef={f(),f(),f()};
  for(auto& a:box.mAxisRef)a={f(),f(),f()};
  // Exercise hidden branches without triggering the legacy empty-visible-box UB.
  cylinder.mVisible=run%5!=0;sphere.mVisible=run%7!=0;
  root.UpdateAll();
  for(auto*n:nodes){fold(&n->mPosition,sizeof(Vector));fold(&n->mBoxSize,sizeof(Vector));fold(&n->mSphereRadius,4);fold(&n->mUpdated,1);}
  fold(&cylinder.mAxis,sizeof(Vector));fold(box.mAxis,sizeof(box.mAxis));fold(&wall.mNormal,sizeof(Vector));
 }
 for(unsigned run=0;run<20000;++run){
  CollisionVolume a,b;a.mBoxSize=b.mBoxSize={3,3,3};
  TList<CollisionVolume*> la,lb;std::vector<CollisionVolume> aa(random()%20),bb(random()%100);
  for(unsigned i=0;i<aa.size();++i){aa[i].mObjRefIndex=i+1;aa[i].mPosition={f()*4,f()*4,f()*4};aa[i].mBoxSize={1,1,1};la.v.push_back(&aa[i]);}
  for(unsigned i=0;i<bb.size();++i){bb[i].mObjRefIndex=i+100;bb[i].mPosition={f()*4,f()*4,f()*4};bb[i].mBoxSize={1,1,1};lb.v.push_back(&bb[i]);}
  WallVolume wa,wb;wa.mType=wb.mType=WallVolumeType;
  wa.mNormal={1,0,0};wb.mNormal={0,0,1};wa.mPosition={f(),f(),f()};wb.mPosition={f(),f(),f()};
  wa.mObjRefIndex=500;wb.mObjRefIndex=501;
  if(run%5==0&&!la.v.empty())la.v[0]=&wa;
  if(run%7==0&&run%5!=0&&!lb.v.empty())lb.v[0]=&wb;
  a.mSubVolumeList=run%3?&la:nullptr;b.mSubVolumeList=&lb;
  a.mType=a.mSubVolumeList?BBoxVolumeType:SphereVolumeType;b.mType=BBoxVolumeType;
  CollisionDetector d;d.DetectOnSubVolume(&a,&b);for(auto v:d.visits)fold(&v,4);
 }
 printf("%llu %u\n",(unsigned long long)hash,boundsTests);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-collision-prep-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+methods+tests);outputs=[]
 for mode in ['reference','prepared']:
  cmd=['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/mode)]
  if mode=='prepared':cmd+=['-DSRR2_OLD3DS_COLL_PREP']
  subprocess.run(cmd,check=True);outputs.append(subprocess.check_output([str(p/mode)]))
 reference,prepared=[x.split() for x in outputs]
 assert reference[0]==prepared[0],outputs
 assert int(prepared[1]) < int(reference[1]),outputs
 print("Bounds checks: reference="+reference[1].decode()+" prepared="+prepared[1].decode())
 print('PASS: 20,000 production volume-tree updates and 20,000 child-list cases; exact geometry/Updated state and ordered candidates, including >64 fallback')
