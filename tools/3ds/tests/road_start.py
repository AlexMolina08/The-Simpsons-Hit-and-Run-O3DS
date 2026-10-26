#!/usr/bin/env python3
"""Check production benchmark placement before warmup and measured driving."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/main/nativebench.cpp').read_text()
start = source.index(' Vehicle* setupCar=GetGameplayManager()->GetCurrentVehicle();')
end = source.index('\n setupCar->Reset(true,false);', start)
placement = source[start:end]
fixture = r'''
#include <cassert>
#include <cmath>
namespace rmt { struct Vector {
 float x=0,y=0,z=0;
 Vector operator-(const Vector& b)const{return {x-b.x,y-b.y,z-b.z};}
 float Dot(const Vector& b)const{return x*b.x+y*b.y+z*b.z;}
 float MagnitudeSqr()const{return Dot(*this);}
}; }
struct RoadSegment {
 void GetPosition(float t,float,rmt::Vector* p){*p={100*t,0,0};}
} segment;
struct Intersect {
 bool loaded=true;
 void FindClosestRoad(const rmt::Vector&,float,RoadSegment*& s,float& d){s=loaded?&segment:nullptr;d=0;}
} intersection;
Intersect* GetIntersectManager(){return &intersection;}
struct Vehicle {
 rmt::Vector position{20,0,10},initial{};float mResetFacingRadians=0;
 rmt::Vector GetPosition(){return position;}
 void SetInitialPositionGroundOffsetAutoAdjust(rmt::Vector* p){initial=*p;}
} car;
struct Gameplay {Vehicle* GetCurrentVehicle(){return &car;}} gameplay;
Gameplay* GetGameplayManager(){return &gameplay;}
struct Character {rmt::Vector facing{1,0,0};void GetFacing(rmt::Vector& out){out=facing;}} character;
struct Avatar {
 rmt::Vector position{25,0,10};
 void GetPosition(rmt::Vector& out){out=position;}
 Character* GetCharacter(){return &character;}
} player;
Avatar* avatar=&player;
bool roadRoute=true,portrait=false,casinoAnchor=false,placementRelocated=false,aborted=false,done=false;
rmt::Vector placementPlayer,placementOriginalCar;
unsigned Place(unsigned dt){
PRODUCTION
 return dt;
}
void Reset(){placementRelocated=aborted=done=false;car.initial={-1,-1,-1};intersection.loaded=true;}
int main(){
 // A nearby parked driveway car still starts a road test on the loaded road.
 Reset();Place(33);assert(placementRelocated && car.initial.x==25 && car.initial.z==0);
 assert(std::fabs(car.mResetFacingRadians-std::atan2(100.f,0.f))<1e-6f);
 // Clamp projection to the road segment; preserve facing direction.
 Reset();player.position.x=120;character.facing.x=-1;Place(33);
 assert(car.initial.x==100 && car.mResetFacingRadians<0);
 player.position={25,0,10};character.facing.x=1;
 // Portraits and anchored views retain their existing nearby-car setup.
 Reset();portrait=true;Place(33);assert(!placementRelocated);portrait=false;
 Reset();casinoAnchor=true;Place(33);assert(!placementRelocated);casinoAnchor=false;
 // A remote car is still brought to the loaded player for other captures.
 Reset();roadRoute=false;car.position.x=1000;Place(33);assert(placementRelocated);
 roadRoute=true;car.position.x=20;
 // Missing road data aborts setup, rather than starting an empty-world run.
 Reset();intersection.loaded=false;Place(33);assert(aborted && done && !placementRelocated);
}
'''.replace('PRODUCTION', placement)
with tempfile.TemporaryDirectory(prefix='srr-road-start-') as directory:
    folder = Path(directory)
    (folder / 'test.cpp').write_text(fixture)
    subprocess.run(['clang++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(folder / 'test.cpp'),
                    '-o', str(folder / 'test')], check=True)
    subprocess.run([str(folder / 'test')], check=True)
print('PASS: production placement starts road tests on nearby roads, preserves portraits/anchors, bounds projection and refuses missing road data.')
