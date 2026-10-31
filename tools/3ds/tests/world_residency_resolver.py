#!/usr/bin/env python3
"""Exercise the production read-only resolver and whole-level admission boundary."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root/'libs/radcore/src/radfile/sdl/sdldrive.cpp').read_text()
a = source.index('#ifdef SRR2_OLD3DS_WORLD_RESIDENCY\n    const char* residentRoot=')
b = source.index('\n#endif', a)
selector = source[a:b]+'\n#endif\n'
code = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <string>
#include <worldresidencyassets.hpp>
const char* g_old3dsResidencyRoot=nullptr;
static bool admitted=true,exists=true;
static unsigned admissionCalls=0,lastLevel=0;
extern "C" bool old3dsWorldResidencyAdmit(unsigned level){++admissionCalls;lastLevel=level;return admitted;}
static constexpr unsigned radFileFilenameMax=128;
enum {OpenExisting,CreateAlways,FileNotFound,Error=-1};
static int m_LastError=0;static std::string opened;
void* SDL_RWFromFile(const char* path,const char*){opened=path;return exists?reinterpret_cast<void*>(1):nullptr;}
static int select(const char* path,int flags=OpenExisting,bool writeAccess=false){
 char fullName[129];snprintf(fullName,sizeof(fullName),"%s",path);
 void* handle=nullptr;void** pHandle=&handle;const char* createFlags="rb";
 static const char dataRoot[]="sdmc:/3ds/simpsons/";
SELECTOR
 return handle?1:0;
}
int main(){
 const char* world="sdmc:/3ds/simpsons/art/l2z2.p3d";
 assert(!select(world)&&opened.empty()&&!admissionCalls);
 g_old3dsResidencyRoot="romfs:/world-residency";
 assert(select(world)==1 && lastLevel==2 && admissionCalls==1);
 assert(opened=="romfs:/world-residency/art/l2z2.p3d");
 assert(select("sdmc:/3ds/simpsons/ART\\l1_terra.P3D")==1 && lastLevel==1);
 assert(opened=="romfs:/world-residency/art/L1_TERRA.p3d");
 admitted=false;opened.clear();assert(!select(world)&&opened.empty());
 assert(!select("sdmc:/3ds/simpsons/art/l2_TERRA.p3d")&&opened.empty());
 admitted=true;
 for(unsigned i=0;i<7;++i){const char* names[]={"L1_TERRA.p3d","l2_TERRA.p3d","l3_TERRA.p3d","L4_TERRA.p3d","l5_TERRA.p3d","l6_TERRA.p3d","L7_TERRA.p3d"};
  std::string name="sdmc:/3ds/simpsons/art/";name+=names[i];assert(select(name.c_str())==1&&lastLevel==i+1);
 }
 opened.clear();const unsigned calls=admissionCalls;
 assert(!select(world,OpenExisting,true)&&opened.empty());
 assert(!select(world,CreateAlways,false)&&opened.empty());
 for(const char* path:{"sdmc:/3ds/simpsons/Save1","sdmc:/other/art/l2z2.p3d","sdmc:/3ds/simpsons/art/l2z2.p3dx","sdmc:/3ds/simpsons/art/../l2z2.p3d","sdmc:/3ds/simpsons/art/l1r4.p3d","sdmc:/3ds/simpsons/art/l3r5_dam.p3d","sdmc:/3ds/simpsons/art/l6r5_dam.p3d"})
  assert(!select(path)&&opened.empty());
 assert(admissionCalls==calls);
 exists=false;assert(select(world)==Error&&m_LastError==FileNotFound);
 assert(opened=="romfs:/world-residency/art/l2z2.p3d");
}
'''.replace('SELECTOR', selector)
with tempfile.TemporaryDirectory(prefix='residency-resolver-') as directory:
    folder = Path(directory)
    (folder/'test.cpp').write_text(code)
    subprocess.run(['clang++', '-std=c++17', '-DSRR2_OLD3DS_WORLD_RESIDENCY',
                    '-I'+str(root/'libs/radcore/inc'), '-fsanitize=address,undefined',
                    str(folder/'test.cpp'), '-o', str(folder/'test')], check=True)
    subprocess.run([str(folder/'test')], check=True)
print('PASS: production resolver admits all maps before first open, rejects mixed fallback, preserves saves/writes and enforces canonical paths/casing.')
