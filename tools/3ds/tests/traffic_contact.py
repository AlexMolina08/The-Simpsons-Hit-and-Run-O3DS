#!/usr/bin/env python3
"""Check traffic contact placement against the production traffic transform."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/worldsim/redbrick/vehicle.cpp').read_text()
transform = source.split('void Vehicle::TrafficSetTransform(')[1]
transform = transform[transform.index('    float restHeightAboveGround'):transform.index('    mSimStateArticulated->SetTransform')]
queue = source.split('    if(picaVisual::Enabled(picaVisual::ContactShadows) && IsSimpleShadow())')[1].split('#endif')[0]
program = r'''
#include <pddi/pica/contactshadow.hpp>
#include <cassert>
#include <cmath>
#include <cstring>
#include <random>
namespace rmt {
struct Vector {
 float x,y,z;
 Vector operator*(float s)const{return {x*s,y*s,z*s};}
 void operator-=(Vector v){x-=v.x;y-=v.y;z-=v.z;}
 void Scale(float s){x*=s;y*=s;z*=s;}
 void Add(Vector v){x+=v.x;y+=v.y;z+=v.z;}
};
float Max(float a,float b){return a>b?a:b;}
float Clamp(float a,float lo,float hi){return a<lo?lo:(a>hi?hi:a);}
}
struct Matrix {
 rmt::Vector rows[4];
 const rmt::Vector& Row(unsigned i)const{return rows[i];}
 void FillTranslate(rmt::Vector v){rows[3]=v;}
};
struct Plane {rmt::Vector mNormal,mPosition;};
constexpr unsigned VL_TRAFFIC=1;
rmt::Vector receivedCenter;
float receivedAlpha;
unsigned queued;
void picaContactQueue(const float* c,const float*,const float*,float,float,float a){
 receivedCenter={c[0],c[1],c[2]};receivedAlpha=a;++queued;
}
struct VehicleProbe {
 Matrix mTransform;
 rmt::Vector mVehicleUp;
 Plane plane;
 Plane* mGroundPlaneWallVolume=&plane;
 unsigned locomotion=VL_TRAFFIC,restCalls=0;
 float rest=0;
 unsigned GetLocomotionType(){return locomotion;}
 float GetRestHeightAboveGround(){++restCalls;return rest;}
 void Place(Matrix& m){
 TRANSFORM
  mTransform=m;
 }
 void Draw(){
 QUEUE
 }
};
void close(rmt::Vector a,rmt::Vector b){
 assert(std::fabs(a.x-b.x)<.00005f);
 assert(std::fabs(a.y-b.y)<.00005f);
 assert(std::fabs(a.z-b.z)<.00005f);
}
int main(){
 std::mt19937 random(251025);
 std::uniform_real_distribution<float> value(-1,1);
 for(unsigned trial=0;trial<5000;++trial){
  VehicleProbe v;
  rmt::Vector road={100*value(random),100*value(random),100*value(random)};
  rmt::Vector up={.6f*value(random),1,.6f*value(random)};
  up.Scale(1/std::sqrt(up.x*up.x+up.y*up.y+up.z*up.z));
  v.mVehicleUp=up;v.rest=.2f+1.8f*(value(random)+1)*.5f;
  Matrix m={{{1,0,0},up,{0,0,1},road}};
  v.Place(m);v.restCalls=queued=0;
  const Matrix before=v.mTransform;
  v.Draw();
  assert(queued==1 && v.restCalls==1);
  close(receivedCenter,road);
  assert(receivedAlpha==.48f);
  assert(!std::memcmp(&before,&v.mTransform,sizeof(before)));

  v.locomotion=0;v.restCalls=queued=0;
  v.plane.mNormal=up;v.plane.mPosition=road;
  const float air=(value(random)+1)*2;
  v.mTransform.rows[3]={road.x,road.y+v.rest+air,road.z};
  v.Draw();
  assert(queued==1 && v.restCalls==1);
  close(receivedCenter,road);
  assert(std::fabs(receivedAlpha-.48f*(1-air*.25f))<.00001f);
 }
}
'''.replace('TRANSFORM', transform).replace('QUEUE', queue)
with tempfile.TemporaryDirectory(prefix='srr-traffic-contact-') as directory:
    folder = Path(directory)
    (folder / 'test.cpp').write_text(program)
    subprocess.run(['clang++', '-std=c++14', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I'+str(root/'libs/pure3d'),
                    str(folder/'test.cpp'), '-o', str(folder/'test')], check=True)
    subprocess.run([str(folder/'test')], check=True)
print('PASS: 5000 production traffic transforms and 5000 cached physics-plane contacts; vehicle transforms unchanged.')
