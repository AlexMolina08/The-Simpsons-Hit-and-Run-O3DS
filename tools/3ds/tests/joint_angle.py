#!/usr/bin/env python3
"""Compare production joint-angle dead-zone handling, including float boundaries."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/sim/simphysics/physicsjoint.cpp').read_text()
a=s.index('void PhysicsJoint::JointAngle(');b=s.index('\nfloat PhysicsJoint::TryAddValue',a);method=s[a:b]
harness=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <random>
unsigned calls=0;
float ACosSafe(float a){if(a>=1.0f)return 0;++calls;return acosf(a);}
float Fabs(float a){return fabsf(a);}
struct Vector{float x=0,y=0,z=0;float DotProduct(const Vector& b)const{return x*b.x+y*b.y+z*b.z;}};
struct Parent{Vector axis;void JointAxis(int,Vector& out,bool){out=axis;}};
'''
classes=r'''
float MIN_ANGLE_ACCURACY=0.1f;
struct PhysicsJoint{Parent* mParentObject;int mParentIndex=0;Vector mAxis;float deformation;
 float Deformation(){return deformation;}void JointAngle(bool,float&);};
'''
tests=r'''
int main(){
 Parent p;p.axis.x=1;
 Reference::PhysicsJoint a;Optimized::PhysicsJoint b;a.mParentObject=b.mParentObject=&p;
 unsigned cases=0,refCalls=0,fastCalls=0;
 auto test=[&](float x){a.mAxis=b.mAxis={x,0,0};a.deformation=b.deformation=x;
  for(bool parent:{false,true}){float av,bv;calls=0;a.JointAngle(parent,av);refCalls+=calls;
   calls=0;b.JointAngle(parent,bv);fastCalls+=calls;assert(!memcmp(&av,&bv,sizeof(av)));++cases;}};
 // Every representable normal float from below the guard through one.
 for(uint32_t bits=0x3f7f0000;bits<=0x3f800010;++bits){float x;memcpy(&x,&bits,4);test(x);}
 std::mt19937 rng(1513);
 for(unsigned i=0;i<1000000;++i){uint32_t bits=rng();float x;memcpy(&x,&bits,4);test(x);}
 // Debug threshold changes must retain the reference's smaller dead zone.
 for(float tolerance:{0.0f,0.01f,0.044f,0.1f,0.2f}){
  Reference::MIN_ANGLE_ACCURACY=Optimized::MIN_ANGLE_ACCURACY=tolerance;
  for(float x:{-1.1f,-1.0f,0.0f,.995f,.9989999f,.999f,.99901f,1.0f,1.1f})test(x);
 }
 assert(fastCalls<refCalls);
 printf("PASS: %u production joint-angle comparisons; %u reference / %u optimized acos calls; exact bits, dead-zone boundaries, NaN/Inf and live tolerances\n",cases,refCalls,fastCalls);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-joint-angle-') as td:
 p=Path(td);(p/'test.cpp').write_text(harness+'namespace Reference {\n'+classes+method+'}\n#define SRR2_OLD3DS_JOINT_ANGLE_FAST\nnamespace Optimized {\n'+classes+method+'}\n'+tests)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
