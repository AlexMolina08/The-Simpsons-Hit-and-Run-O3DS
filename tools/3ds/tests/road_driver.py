#!/usr/bin/env python3
"""Exercise the production road planner and controller at route boundaries."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/main/nativebench.cpp').read_text()
a=source.index('static bool BuildRoadRoute(');b=source.index('\nstatic unsigned saveFixture',a)
fixture=r'''
#include <cassert>
#include <cmath>
#include <vector>
namespace rmt {struct Vector {float x=0,y=0,z=0;
 Vector operator-(const Vector& b)const{return {x-b.x,y-b.y,z-b.z};}
 float MagnitudeSqr()const{return x*x+y*y+z*z;}float Magnitude()const{return std::sqrt(MagnitudeSqr());}};}
struct Road;
struct RoadSegment {Road* road; rmt::Vector a,b;
 void GetPosition(float t,float,rmt::Vector* out)const{*out={a.x+t*(b.x-a.x),a.y+t*(b.y-a.y),a.z+t*(b.z-a.z)};}
 const Road* GetRoad()const{return road;}int GetSegmentIndex()const{return 0;}};
struct Intersection {std::vector<Road*> in,out;
 unsigned GetNumRoadsIn()const{return in.size();}unsigned GetNumRoadsOut()const{return out.size();}
 const Road* GetRoadIn(unsigned i)const{return in[i];}const Road* GetRoadOut(unsigned i)const{return out[i];}};
struct Road {Intersection* source;Intersection* destination;RoadSegment segment;bool shortcut=false;
 unsigned GetNumRoadSegments()const{return 1;}RoadSegment* GetRoadSegment(unsigned)const{return const_cast<RoadSegment*>(&segment);}
 const Intersection* GetSourceIntersection()const{return source;}const Intersection* GetDestinationIntersection()const{return destination;}
 bool GetShortCut()const{return shortcut;}};
struct Vehicle {rmt::Vector position{5,0,0},facing{1,0,0};float mSpeed=10;
 const rmt::Vector& GetPosition()const{return position;}const rmt::Vector& GetFacing()const{return facing;}};
struct Intersect {RoadSegment* start=nullptr;
 void FindClosestRoad(const rmt::Vector&,int,RoadSegment*& out,float& distance){out=start;distance=0;}} manager;
Intersect* GetIntersectManager(){return &manager;}
static rmt::Vector roadPoints[513],roadLastPosition;
static unsigned roadPointCount=0,roadPoint=0,roadStalledMs=0,roadReverseMs=0,roadRecoveries=0,roadBuiltCount=0,seconds=120;
static float roadDistance=0,roadPlannedDistance=0;
static int roadSteer=0,roadGas=0,roadBrake=0;
static bool roadPathExhausted=false,aborted=false,done=false;
PRODUCTION
void Reset(){roadPointCount=roadPoint=roadBuiltCount=0;roadDistance=roadPlannedDistance=0;roadPathExhausted=aborted=done=false;roadPoints[512]={123,456,789};}
int main(){
 Vehicle car;Reset();assert(!BuildRoadRoute(&car));
 Intersection a,b,c;Road first{&a,&b,{nullptr,{0,0,0},{25,0,0}}};first.segment.road=&first;
 a.out={&first};b.in={&first};manager.start=&first.segment;
 // A genuine dead end must turn back rather than end after two points.
 Reset();assert(BuildRoadRoute(&car));assert(roadBuiltCount==128 && roadPointCount<=512 && roadPlannedDistance>=2360);
 assert(roadPoints[512].x==123 && roadPoints[512].y==456 && roadPoints[512].z==789);
 // Prefer an ordinary continuation to the U-turn fallback.
 Road next{&b,&c,{nullptr,{25,0,0},{50,0,0}}};next.segment.road=&next;b.out={&next};c.in={&next};
 Reset();assert(BuildRoadRoute(&car));assert(roadPoints[1].x==25 && roadPoints[2].x==50);
 next.shortcut=true;Reset();assert(BuildRoadRoute(&car));assert(roadPoints[2].x==0);
 // Refuse a route too short for the requested driving time.
 first.segment.b.x=.1f;Reset();assert(!BuildRoadRoute(&car));first.segment.b.x=25;
 // Reaching the final target fails visibly instead of circling it forever.
 Reset();roadPointCount=2;roadPoint=1;roadPoints[1]=car.position;
 UpdateRoadInput(&car,33);assert(roadPathExhausted && aborted && done && !roadGas && roadBrake);
 Reset();roadPointCount=2;roadPoint=1;roadPoints[1]={100,0,0};UpdateRoadInput(&car,33);
 assert(!roadPathExhausted && !aborted && roadGas && !roadBrake);
}
'''.replace('PRODUCTION',source[a:b])
with tempfile.TemporaryDirectory(prefix='srr-road-driver-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: production route planning handles dead ends, prefers normal roads, bounds point storage, rejects short paths and fails an exhausted route without circling.')
