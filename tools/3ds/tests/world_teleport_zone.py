#!/usr/bin/env python3
"""Round-trip authored exterior operations through the actual engine parser."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/meta/zoneeventlocator.cpp').read_text()
a=source.index('void ZoneEventLocator::SetZone( ');b=source.index('//=============================================================================',a)
parser=source[a:b].replace('ZoneEventLocator::','Zone::')
code=r'''#include <main/worldstressplan.h>
#include <cassert>
#include <cstring>
#include <vector>
#define rAssert assert
struct Slots{std::vector<int> items;unsigned capacity=0;void Allocate(unsigned n){capacity=n;}void Add(int n){assert(items.size()<capacity);items.push_back(n);}int operator[](int n){return items.at(n);}};
struct Zone{
 unsigned mZoneSize=255;char mZone[255]={};Slots mLoadZones,mDumpZones,mLWSActivates,mLWSDeactivates;bool mInteriorLoad=false,mInteriorDump=false;int mInteriorSection=0;
 bool IsInteriorLoad(){return mInteriorLoad;}bool IsInteriorDump(){return mInteriorDump;}
 int GetNumLoadZones(){return mLoadZones.items.size();}int GetNumDumpZones(){return mDumpZones.items.size();}
 int GetNumLWSActivates(){return mLWSActivates.items.size();}int GetNumLWSDeactivates(){return mLWSDeactivates.items.size();}
 const char* GetLoadZone(int n){return mZone+mLoadZones[n];}const char* GetDumpZone(int n){return mZone+mDumpZones[n];}
 const char* GetLWSActivates(int n){return mZone+mLWSActivates[n];}const char* GetLWSDeactivates(int n){return mZone+mLWSDeactivates[n];}
 void SetZone(const char*);
};
'''+parser+r'''
int main(){
 Zone source;source.SetZone("l4r4a.p3d;l4r4b.p3d;l4z2.p3d:l4z3.p3d:powerplant*school&");
 assert(source.GetNumLoadZones()==2 && !strcmp(source.mZone,"l4r4a.p3d"));
 char text[254];assert(WorldStress::ExteriorZoneText(&source,text));Zone copy;copy.SetZone(text);
 assert(copy.GetNumLoadZones()==2 && copy.GetNumDumpZones()==2 && copy.GetNumLWSActivates()==1 && copy.GetNumLWSDeactivates()==1);
 for(int i=0;i<2;++i){assert(!strcmp(source.GetLoadZone(i),copy.GetLoadZone(i)));assert(!strcmp(source.GetDumpZone(i),copy.GetDumpZone(i)));}
 assert(!strcmp(source.GetLWSActivates(0),copy.GetLWSActivates(0)) && !strcmp(source.GetLWSDeactivates(0),copy.GetLWSDeactivates(0)));
 source.mInteriorLoad=true;assert(!WorldStress::ExteriorZoneText(&source,text));
 source.mInteriorLoad=false;memset(source.mZone,'x',254);source.mZone[254]=0;assert(!WorldStress::ExteriorZoneText(&source,text));
}
'''
with tempfile.TemporaryDirectory(prefix='world-teleport-zone-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual destructive zone parser round-trips loads, dumps and LWS operations; interior and overflow rejection.')
