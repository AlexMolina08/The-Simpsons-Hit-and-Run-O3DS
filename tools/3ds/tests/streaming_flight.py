#!/usr/bin/env python3
"""Exercise the production focus sweep through corners without loading barriers."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/main/nativebench.cpp').read_text()
start = source.index('static void UpdateStreamingFlight(Avatar* avatar,unsigned dt)')
end = source.index('static unsigned hashFloat', start)
function = source[start:end]
prelude = r'''
#include <cassert>
#include <cmath>
namespace rmt {struct Vector {
 float x,y,z;Vector(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
 void Set(float a,float b,float c){x=a;y=b;z=c;}
 float& operator[](unsigned i){return (&x)[i];}
 float operator[](unsigned i)const{return (&x)[i];}
 Vector operator+(Vector v)const{return {x+v.x,y+v.y,z+v.z};}
 Vector operator-(Vector v)const{return {x-v.x,y-v.y,z-v.z};}
 Vector operator-()const{return {-x,-y,-z};}
 Vector operator*(float f)const{return {x*f,y*f,z*f};}
 Vector& operator+=(Vector v){x+=v.x;y+=v.y;z+=v.z;return *this;}
 float MagnitudeSqr()const{return x*x+y*y+z*z;}
};}
struct Avatar{};struct Vehicle{rmt::Vector GetPosition(){return {};}};
struct SuperCam {enum {STATIC_CAM};};
struct StaticCam:SuperCam {
 rmt::Vector position,target;
 void SetTarget(Vehicle*){}void SetTracking(bool){}
 void SetPosition(rmt::Vector p){position=p;}void SetTargetOffset(rmt::Vector p){target=p;}
 void SetTargetLag(float){}void SetMaxFOV(float){}void SetFOV(float){}
};
struct SuperCamCentral {
 enum {CUT=1,FORCE=2};StaticCam* camera=nullptr;SuperCam* active=nullptr;
 SuperCam* GetSuperCam(int){return camera;}SuperCam* GetActiveSuperCam(){return active;}
 void RegisterSuperCam(StaticCam* c){camera=c;}void SelectSuperCam(SuperCam* c,int){active=c;}
};
struct Cameras {SuperCamCentral central;SuperCamCentral* GetSCC(int){return &central;}} cameras;
struct Gameplay {Vehicle vehicle;Vehicle* GetCurrentVehicle(){return &vehicle;}} gameplay;
Gameplay* GetGameplayManager(){return &gameplay;}Cameras* GetSuperCamManager(){return &cameras;}
namespace WorldStreaming {
 bool flightActive=false,cameraSweep=true;unsigned incidentRoute=0;float flightFov=1.15f,flightSpeed=32,flightPosition[3],flightVelocity[3];Avatar* flightAvatar=nullptr;
}
static unsigned frame=0,roadPoint=1,roadPointCount=5,elapsedMs=0;
static rmt::Vector roadPoints[]={{0,0,0},{100,0,0},{100,0,100},{0,0,100},{0,0,0}};
static float roadDistance=0;static bool roadPathExhausted=false,aborted=false,done=false;
'''
test = r'''
int main(){
 Avatar avatar;UpdateStreamingFlight(&avatar,100);
 assert(roadDistance==0&&WorldStreaming::flightAvatar==&avatar);
 for(unsigned i=1;i<125;++i){
  frame=i;elapsedMs=i*100;UpdateStreamingFlight(&avatar,100);
  assert(!aborted&&!roadPathExhausted);
  const float distance=i*3.2f;
  assert(std::fabs(roadDistance-distance)<.001f);
  rmt::Vector expected;
  if(distance<100)expected={distance,0,0};
  else if(distance<200)expected={100,0,distance-100};
  else if(distance<300)expected={300-distance,0,100};
  else expected={0,0,400-distance};
  for(unsigned axis=0;axis<3;++axis)assert(std::fabs(WorldStreaming::flightPosition[axis]-expected[axis])<.001f);
  const auto* camera=cameras.central.camera;assert(camera&&cameras.central.active==camera);
  const rmt::Vector view=camera->target-camera->position;
  assert(std::fabs(view.x*view.x+view.z*view.z-576)<.01f);
 }
 frame=126;UpdateStreamingFlight(&avatar,200);
 assert(aborted&&done&&roadPathExhausted);
 delete cameras.central.camera;
}
'''
with tempfile.TemporaryDirectory(prefix='streaming-flight-') as directory:
    p = Path(directory)
    (p / 'test.cpp').write_text(prelude + function + test)
    subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined',
                    str(p / 'test.cpp'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('PASS: production flight moves focus at measured distance through three corners, matches camera and stops at exhausted paths.')
