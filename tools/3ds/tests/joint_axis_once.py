#!/usr/bin/env python3
"""Check production joint-angle calculations and live constraint handling."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3];s=(root/'libs/sim/simphysics/physicsjoint.cpp').read_text()
def extract(sig):
 a=s.index(sig);e=s.index('{',a)+1;depth=1
 while depth:depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
methods='\n'.join(extract(sig) for sig in ['void PhysicsJoint::JointAngle(bool','void PhysicsJoint1D::ComputeJointAngle()','void PhysicsJoint3D::ComputeJointAngle(const Vector*','void PhysicsJoint1D::TestJointAngle()','void PhysicsJoint3D::TestJointAngle()'])
helper=extract('void PhysicsJoint::JointAngleFromAxis')
prelude=r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cfenv>
#include <cstdint>
#include <initializer_list>
#define SRR2_OLD3DS_JOINT_ANGLE_FAST
float Fabs(float x){return fabsf(x);}float Sqrt(float x){return sqrtf(x);}float Sign(float x){return x<0?-1:1;}
float ACosSafe(float x){if(x>=1)return 0;if(x<=-1)return 3.14159265358979323846f;return acosf(x);}
struct Vector {float x=0,y=0,z=0;
 float DotProduct(const Vector& b)const{return x*b.x+y*b.y+z*b.z;}
 void CrossProduct(const Vector&a,const Vector&b){*this={a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
 float MagnitudeSqr()const{return DotProduct(*this);}float Magnitude()const{return sqrtf(MagnitudeSqr());}
 void Scale(float v){x*=v;y*=v;z*=v;}void ScaleAdd(float scale,const Vector& v){x+=scale*v.x;y+=scale*v.y;z+=scale*v.z;}
};
struct Parent {Vector axis;unsigned calls=0;void JointAxis(int,Vector& out,bool){out=axis;++calls;}};
'''
classes=r'''
float MIN_ANGLE_ACCURACY=.1f,MAXDEFSPEED=4,MAXDEFSPEED2=16,MaxTorsionAngle=.5f,AngleRecoveryRate=.7f;
bool LimitJointAngle=true;unsigned jointAxisReuses1D=0,jointAxisReuses3D=0;
struct PhysicsJoint {Parent* mParentObject;int mParentIndex=0;Vector mAxis;float mMinAngle=-.3f,mMaxAngle=.6f;
 virtual float Deformation()const=0;void JointAngle(bool,float&);
#ifdef SRR2_OLD3DS_JOINT_AXIS_ONCE
 void JointAngleFromAxis(const Vector&,float&);
#endif
};
struct PhysicsJoint1D:PhysicsJoint {bool mParentRelative=false;Vector mRotAxis;float mDeformation=0,mDeformationSpeed=0,mCurrentAngle=0;
 float Deformation()const{return mDeformation;}void ComputeJointAngle();void TestJointAngle();};
struct PhysicsJoint3D:PhysicsJoint {bool mParentRelative=false;Vector mDeformation,mDeformationSpeed,mCurrentRotAxis;float mCurrentAngle=0,mCurrentTorsionAngle=0;
 float Deformation()const{return mDeformation.Magnitude();}void ComputeJointAngle(const Vector* preparedAxis=nullptr);void TestJointAngle();};
'''
code=prelude+'namespace Reference{\n'+classes+methods+'}\n#define SRR2_OLD3DS_JOINT_AXIS_ONCE\nnamespace Candidate{\n'+classes+helper+methods+'}\n'+r'''
int main(){
 unsigned rng=172;auto word=[&](){rng=rng*1664525u+1013904223u;return rng;};auto value=[&](){unsigned bits=word();float v;memcpy(&v,&bits,4);return v;};
 unsigned cases=0,referenceCalls=0,candidateCalls=0;
 for(int round:{FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){
  assert(!fesetround(round));
  for(unsigned n=0;n<12000;++n){
   Parent p;p.axis={value(),value(),value()};Vector axis{value(),value(),value()},rot{value(),value(),value()};
   Reference::LimitJointAngle=Candidate::LimitJointAngle=(n&1);Reference::MIN_ANGLE_ACCURACY=Candidate::MIN_ANGLE_ACCURACY=n%5==0?.01f:.1f;
   Reference::PhysicsJoint1D a;Candidate::PhysicsJoint1D b;a.mParentObject=b.mParentObject=&p;a.mAxis=b.mAxis=axis;a.mRotAxis=b.mRotAxis=rot;
   a.mParentRelative=b.mParentRelative=n&2;a.mDeformation=b.mDeformation=value();a.mDeformationSpeed=b.mDeformationSpeed=value();
   p.calls=0;a.ComputeJointAngle();a.TestJointAngle();referenceCalls+=p.calls;
   p.calls=0;b.ComputeJointAngle();b.TestJointAngle();candidateCalls+=p.calls;
   assert(!memcmp(&a.mCurrentAngle,&b.mCurrentAngle,4));assert(!memcmp(&a.mDeformation,&b.mDeformation,4));assert(!memcmp(&a.mDeformationSpeed,&b.mDeformationSpeed,4));++cases;
   Reference::PhysicsJoint3D c;Candidate::PhysicsJoint3D d;c.mParentObject=d.mParentObject=&p;c.mAxis=d.mAxis=axis;c.mCurrentRotAxis=d.mCurrentRotAxis=rot;
   c.mParentRelative=d.mParentRelative=n&2;c.mDeformation=d.mDeformation={value(),value(),value()};c.mDeformationSpeed=d.mDeformationSpeed={value(),value(),value()};
   p.calls=0;c.ComputeJointAngle();c.TestJointAngle();referenceCalls+=p.calls;
   p.calls=0;d.ComputeJointAngle(&p.axis);d.TestJointAngle();candidateCalls+=p.calls;
   assert(!memcmp(&c.mCurrentAngle,&d.mCurrentAngle,4));assert(!memcmp(&c.mCurrentTorsionAngle,&d.mCurrentTorsionAngle,4));assert(!memcmp(&c.mDeformation,&d.mDeformation,12));assert(!memcmp(&c.mDeformationSpeed,&d.mDeformationSpeed,12));++cases;
   // Other 3D callers without a prepared axis retain the normal fetch path.
   p.calls=0;c.ComputeJointAngle();unsigned original=p.calls;p.calls=0;d.ComputeJointAngle();assert(p.calls==original);assert(!memcmp(&c.mCurrentAngle,&d.mCurrentAngle,4));
  }
 }
 assert(candidateCalls<referenceCalls);
 printf("PASS: %u production 1D/3D angle and constraint comparisons; four rounding modes, limits, nonfinite values and unprepared fallback; parent-axis lookups %u / %u, not native speed gains\n",cases,referenceCalls,candidateCalls);
}
'''
assert 'ComputeJointAngle(&parentAxis);' in extract('void PhysicsJoint3D::SetTransform')
with tempfile.TemporaryDirectory(prefix='srr-joint-axis-') as td:
 d=Path(td);(d/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-frounding-math','-Wall','-Wextra','-Werror','-Wno-unused-variable','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(d/'test.cpp'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
