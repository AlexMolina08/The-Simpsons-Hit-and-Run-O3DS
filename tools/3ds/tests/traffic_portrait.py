#!/usr/bin/env python3
"""Exercise production traffic selection so wheel controls use the requested car."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
source=(root/'code/main/nativebench.cpp').read_text()
start=source.index(' if(portrait && portraitTraffic && portraitVehicleIndex<0){')
end=source.index('\n  if(portraitNPC){',start)
selection=source[start:end]
fixture=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
namespace rmt {
struct Vector {float x=0;Vector operator-(Vector b)const{return {x-b.x};}float MagnitudeSqr()const{return x*x;}};
}
enum {VT_TRAFFIC,VT_USER};
struct Vehicle {
 const char* name;float distance;int mVehicleType=VT_TRAFFIC;
 const char* GetName(){return name;}rmt::Vector GetPosition(){return {distance};}
};
Vehicle playerCar={"player",0,VT_USER},sedan={"sedanB",2},taxi={"taxiA",7},farTaxi={"taxiA",40};
struct Central {
 Vehicle* cars[5]={&playerCar,&sedan,&taxi,&farTaxi,nullptr};
 int GetMaxActiveVehicles(){return 5;}Vehicle* GetVehicle(int i){return cars[i];}
} central;
Central* GetVehicleCentral(){return &central;}
struct Gameplay {Vehicle* GetCurrentVehicle(){return &playerCar;}} gameplay;
Gameplay* GetGameplayManager(){return &gameplay;}
struct Avatar {void GetPosition(rmt::Vector& p){p.x=0;}} avatarStorage;
Avatar* avatar=&avatarStorage;
bool portrait=true,portraitTraffic=true,aborted=false,done=false;
unsigned stableMs=10000;int portraitVehicleIndex=-1;Vehicle* portraitVehicle=nullptr;
char portraitModel[32]={},portraitVehicleName[32]={};
float select(){const float dt=0;
SELECTION
return dt;}
void reset(){portraitVehicleIndex=-1;portraitVehicle=nullptr;aborted=done=false;}
int main(){
 select();assert(portraitVehicle==&sedan); // Default still chooses nearest traffic.
 reset();strcpy(portraitModel,"taxiA");select();
 assert(portraitVehicle==&taxi && portraitVehicleIndex==2 && !strcmp(portraitVehicleName,"taxiA"));
 reset();central.cars[2]=nullptr;select();assert(!portraitVehicle && !done); // Far taxi cannot substitute.
 stableMs=21000;select();assert(done && aborted && !portraitVehicle);
 reset();stableMs=10000;central.cars[2]=&taxi;portraitTraffic=false;select();assert(!portraitVehicle);
}
'''.replace('SELECTION',selection)
with tempfile.TemporaryDirectory(prefix='srr-traffic-portrait-') as directory:
    p=Path(directory);(p/'test.cpp').write_text(fixture)
    subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: actual nearest-traffic selection honors exact model, range, missing-model abort and disabled mode.')
