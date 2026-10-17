#!/usr/bin/env python3
"""Compare production joint-vector preparation, retaining derived joint checks."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/sim/simphysics/physicsjoint.cpp').read_text()
m=(root/'libs/radmath/radmath/matrix.cpp').read_text()
def extract(text,signature):
 a=text.index(signature+'\n{');b=text.index('{',a)+1;e=b;depth=1
 while depth:
  depth+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[a:e]
base=extract(s,'void PhysicsJoint::SetTransform(const Matrix& m)')
derived=extract(s,'void PhysicsJoint1D::SetTransform(const Matrix& m)')
rotate=extract(m,'void Matrix::RotateVector(const Vector& src, Vector* dest) const')
prelude=r'''
#pragma STDC FENV_ACCESS ON
#include <simphysics/jointvectorcache.hpp>
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <limits>
struct Vector {float x=0,y=0,z=0;void Set(float a,float b,float c){x=a;y=b;z=c;}};
static unsigned rotates;
struct Matrix {float m[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};void RotateVector(const Vector&,Vector*)const;};
'''+rotate.replace('{\n','{\n ++rotates;\n',1)+r'''
struct Info {Vector mAxis{0,1,0},mRotAxis{1,0,0};};
struct Parent {Info info;Matrix matrix;const Info& JointInfo(int)const{return info;}
 const Matrix& JointTransform(int)const{return matrix;}};
'''
classes=r'''
static unsigned vectorRotationCalls=0,vectorRotationHits=0;
namespace jointVectorCache=sim::jointVectorCache;
struct PhysicsJoint {
 Parent* mParentObject;int mIndex=0,mParentIndex=0;
 Matrix mTransform;Vector mDCMPositioni{1,2,3},mDCMPosition,mAxis;
#ifdef SRR2_OLD3DS_JOINT_VECTOR_REUSE
 jointVectorCache::Cache mVectorRotationCache;
#endif
 void SetTransform(const Matrix&);
};
struct PhysicsJoint1D:PhysicsJoint {
 Vector mRotAxis;unsigned angles=0,tests=0;
 void ComputeJointAngle(){++angles;}void TestJointAngle(){++tests;}
 void SetTransform(const Matrix&);
};
'''
code=prelude+'namespace Reference {\n'+classes+base+derived+'}\n#define SRR2_OLD3DS_JOINT_VECTOR_REUSE\nnamespace Optimized {\n'+classes+base+derived+'}\n'+r'''
int main(){
 uint32_t rng=391;auto word=[&](){rng=rng*1664525u+1013904223u;return rng;};
 unsigned original=0,reused=0,total=0;
 for(int mode:{FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){
  assert(!fesetround(mode));Parent parent;Reference::PhysicsJoint1D a;Optimized::PhysicsJoint1D b;
  a.mParentObject=b.mParentObject=&parent;Matrix input;
  for(unsigned n=0;n<12000;++n){
   if(n%7==0)for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)input.m[r][c]=float(int(word()%4096)-2048)/1024.f;
   for(unsigned c=0;c<4;++c)input.m[3][c]=float(int(word()%32768)-16384);
   if(n%11==0)a.mDCMPositioni.x=float(int(word()%4096)-2048)/128.f;
   if(n%13==0)parent.info.mAxis.y=float(int(word()%1024)-512)/128.f;
   if(n%17==0)input.m[0][0]=-0.f;
   if(n%233==0)input.m[1][1]=std::numeric_limits<float>::infinity();
   if(n%239==0)input.m[2][0]=std::numeric_limits<float>::quiet_NaN();
   b.mDCMPositioni=a.mDCMPositioni;
   rotates=0;a.SetTransform(input);original+=rotates;
   rotates=0;b.SetTransform(input);reused+=rotates;++total;
   assert(!memcmp(&a.mTransform,&b.mTransform,64));
   assert(!memcmp(&a.mDCMPosition,&b.mDCMPosition,12)&&!memcmp(&a.mAxis,&b.mAxis,12));
   assert(!memcmp(&a.mRotAxis,&b.mRotAxis,12));assert(a.angles==b.angles&&a.tests==b.tests);
   if(n%101==0){
    auto clone=b;auto reference=a;
    clone.SetTransform(clone.mTransform);reference.SetTransform(reference.mTransform);
    assert(!memcmp(&clone.mDCMPosition,&reference.mDCMPosition,12));
    assert(!memcmp(&clone.mAxis,&reference.mAxis,12));assert(clone.angles==reference.angles&&clone.tests==reference.tests);
   }
  }
 }
 assert(reused<original);assert(sizeof(Optimized::PhysicsJoint1D)<=sizeof(Reference::PhysicsJoint1D)+32);
 sim::jointVectorCache::Cache cache;Vector center,axis;Matrix x,y;
 cache.Store(&center,&axis,0x03000000,true);assert(cache.Matches(x.m,y.m,&center,&axis,0x03000000));
 assert(!cache.Matches(x.m,y.m,&center,&axis,0x03400000));
 cache.Store(&center,&axis,0x03000000,false);assert(!cache.Matches(x.m,y.m,&center,&axis,0x03000000));
 printf("PASS: %u production derived-joint updates, exact transforms/vectors and angle/check counts; clones, aliases, all rounding modes, source changes and nonfinite fallback; RotateVector calls %u reference / %u reused; 32 bytes per joint\n",total,original,reused);
}
'''
code=code.replace('#include <limits>','#include <limits>\n#include <initializer_list>')
with tempfile.TemporaryDirectory(prefix='srr-joint-vectors-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-Wno-unused-variable','-fsanitize=address,undefined','-I'+str(root/'libs/sim'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
