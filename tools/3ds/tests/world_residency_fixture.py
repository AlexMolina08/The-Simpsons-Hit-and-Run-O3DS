#!/usr/bin/env python3
"""Execute the actual read-only fixture selector under address/UB sanitizers."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/3ds'))
from prepare_world_residency import exterior_files
assert set(re.findall(r'"([^"\n]+\.p3d)"', (root/'libs/radcore/inc/worldresidencyassets.hpp').read_text())) == {
    name for level in range(1, 8) for name in exterior_files(level)}
source = (root / 'libs/radcore/src/radfile/sdl/sdldrive.cpp').read_text()
begin = source.index('    // ResidencyFixtureBegin:')
end = source.index('    // ResidencyFixtureEnd.', begin)
code = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <string>
#include <worldresidencyassets.hpp>
static unsigned g_old3dsWorldResidencyChoice=0,s_residencyOpens[5][2]={};
static constexpr unsigned radFileFilenameMax=128;
enum {OpenExisting,CreateAlways,FileNotFound,Error=-1};
static int m_LastError=0;static bool exists=true;static std::string opened;
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
 assert(select(world)==0&&opened.empty());
 g_old3dsWorldResidencyChoice=1;assert(select(world)==1);
 assert(opened=="sdmc:/3ds/hitrun-dev/world-residency-01/reference/art/l2z2.p3d");
 g_old3dsWorldResidencyChoice=2;assert(select("sdmc:/3ds/simpsons/ART\\L2_TERRA.P3D")==1);
 assert(opened=="sdmc:/3ds/hitrun-dev/world-residency-01/resident/art/l2_TERRA.p3d");
 g_old3dsWorldResidencyChoice=3;assert(select(world)==1);
 assert(opened=="sdmc:/3ds/hitrun-dev/world-residency-04/opaque/art/l2z2.p3d");
 g_old3dsWorldResidencyChoice=4;assert(select("sdmc:/3ds/simpsons/art/L1_TERRA.p3d")==1);
 assert(opened=="sdmc:/3ds/hitrun-dev/world-residency-05/all/art/L1_TERRA.p3d");
 assert(select("sdmc:/3ds/simpsons/art/l4r7.p3d")==1);
 assert(opened=="sdmc:/3ds/hitrun-dev/world-residency-05/all/art/l4r7.p3d");
 assert(select("sdmc:/3ds/simpsons/art/L7_TERRA.p3d")==1);
 assert(opened=="sdmc:/3ds/hitrun-dev/world-residency-05/all/art/L7_TERRA.p3d");
 opened.clear();
 assert(!select("sdmc:/3ds/simpsons/art/l3r5_dam.p3d")&&opened.empty());
 assert(!select("sdmc:/3ds/simpsons/art/l6r5_dam.p3d")&&opened.empty());
 assert(!select(world,OpenExisting,true)&&opened.empty());
 assert(!select(world,CreateAlways,false)&&opened.empty());
 assert(!select("sdmc:/3ds/simpsons/Save1")&&opened.empty());
 assert(!select("sdmc:/other/art/l2z2.p3d")&&opened.empty());
 assert(!select("sdmc:/3ds/simpsons/art/l2z2.p3dx")&&opened.empty());
 assert(!select("sdmc:/3ds/simpsons/art/../l2z2.p3d")&&opened.empty());
 exists=false;assert(select(world)==Error&&m_LastError==FileNotFound);
 assert(s_residencyOpens[1][1]==1&&s_residencyOpens[2][0]==1&&s_residencyOpens[2][1]==0&&s_residencyOpens[3][1]==1);
}
'''.replace('SELECTOR', source[begin:end])
with tempfile.TemporaryDirectory(prefix='residency-fixture-') as directory:
    path = Path(directory)
    (path / 'test.cpp').write_text(code)
    subprocess.run(['clang++', '-std=c++17', '-I'+str(root/'libs/radcore/inc'), '-fsanitize=address,undefined',
                    str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: actual fixture selector preserves saves/writes/defaults, requires an exact asset path and fails closed on missing data.')
