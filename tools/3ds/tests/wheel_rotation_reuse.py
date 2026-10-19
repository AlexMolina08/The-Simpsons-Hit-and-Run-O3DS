#!/usr/bin/env python3
"""Execute the production suspension driver and rotation math against the reference."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/worldsim/redbrick/suspensionjointdriver.cpp').read_text()
matrix=(root/'libs/radmath/radmath/matrix.cpp').read_text()
header=(root/'code/worldsim/redbrick/suspensionjointdriver.h').read_text()
header=header[header.index('class SuspensionJointDriver :'):header.index('#endif // #ifndef')]
def extract(source,signature):
 a=source.index(signature);b=source.index('{',a)+1;e=b;depth=1
 while depth:
  depth+=(source[e]=='{')-(source[e]=='}');e+=1
 return source[a:e]
prelude=r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <limits>
static unsigned trigCalls;
namespace rmt {
void SinCos(float a,float* s,float* c){++trigCalls;*s=sinf(a);*c=cosf(a);}
struct Matrix {float m[4][4]={};void FillRotateX(float);void FillRotateXYZ(float,float,float);};
'''+extract(matrix,'void Matrix::FillRotateX(float angle)')+extract(matrix,'void Matrix::FillRotateXYZ(float anglex, float angley, float anglez)')+r'''
}
class Wheel {public:bool mSteerWheel=false;float mCumulativeRot=0,mWheelTurnAngle=0;};
namespace poser {
class PoseDriver {public:virtual ~PoseDriver(){};};
struct Joint {rmt::Matrix matrix;unsigned writes=0;const rmt::Matrix& GetObjectMatrix(){return matrix;}
 void SetObjectMatrix(const rmt::Matrix& m){matrix=m;++writes;}};
struct Pose {Joint joint;Joint* GetJoint(int){return &joint;}};
}
'''
methods='\n'.join(extract(source,x) for x in ['SuspensionJointDriver::SuspensionJointDriver(', 'SuspensionJointDriver::~SuspensionJointDriver()', 'void SuspensionJointDriver::Update('])
code=prelude+'namespace Reference {\n'+header+methods+'\n}\n#define SRR2_OLD3DS_WHEEL_ROTATION_REUSE\nnamespace Optimized {\n'+header+methods+r'''
}
int main(){
 Wheel wheel;Reference::SuspensionJointDriver a(&wheel,0);Optimized::SuspensionJointDriver b(&wheel,0);
 poser::Pose x,y;uint32_t random=1999;auto next=[&](){random=random*1664525u+1013904223u;return random;};
 unsigned referenceCalls=0,cachedCalls=0;
 for(unsigned n=0;n<20000;++n){
  for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){uint32_t bits=next();memcpy(&x.joint.matrix.m[r][c],&bits,4);}
  y.joint.matrix=x.joint.matrix;
  if(n%7==0)wheel.mCumulativeRot=float(int(next()%100000)-50000)*.001f;
  if(n%3==0)wheel.mWheelTurnAngle=float(int(next()%10000)-5000)*.001f;
  if(n%11==0)wheel.mSteerWheel=!wheel.mSteerWheel;
  if(n%17==0)wheel.mCumulativeRot=n%34?-0.0f:0.0f;
  if(n%233==0)wheel.mCumulativeRot=std::numeric_limits<float>::infinity();
  if(n%239==0)wheel.mWheelTurnAngle=std::numeric_limits<float>::quiet_NaN();
  trigCalls=0;a.Update(&x);referenceCalls+=trigCalls;
  trigCalls=0;b.Update(&y);cachedCalls+=trigCalls;
  assert(!memcmp(&x.joint.matrix,&y.joint.matrix,sizeof(rmt::Matrix)));
  assert(x.joint.writes==n+1&&y.joint.writes==n+1);
 }
 assert(cachedCalls<referenceCalls);assert(sizeof(b)<=sizeof(a)+64);
 printf("PASS: 20,000 production suspension updates; exact 16-float matrices and joint writes; SinCos calls %u reference / %u reused; steering modes, signed zero, nonfinite angles and external matrix changes\n",referenceCalls,cachedCalls);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-wheel-rotation-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-variable','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
