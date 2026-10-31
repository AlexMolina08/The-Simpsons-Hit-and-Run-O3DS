#!/usr/bin/env python3
"""Run the actual private asset selector; protect normal paths and writes."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/radcore/src/radfile/sdl/sdldrive.cpp').read_text()
begin = source.index('    const unsigned order=')
end = source.index('\n#endif', begin)
code = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <string>
static unsigned g_old3dsWorldOrderChoice=0,s_orderOpens[3][2]={};
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
 g_old3dsWorldOrderChoice=1;assert(select(world)==1);
 assert(opened=="sdmc:/3ds/hitrun-dev/streaming-order-01/original/l2z2.p3d");
 g_old3dsWorldOrderChoice=2;assert(select("sdmc:/3ds/simpsons/art\\L2R2.P3D")==1);
 assert(opened=="sdmc:/3ds/hitrun-dev/streaming-order-01/ordered/l2r2.p3d");
 opened.clear();
 assert(!select(world,OpenExisting,true)&&opened.empty());
 assert(!select(world,CreateAlways,false)&&opened.empty());
 assert(!select("sdmc:/3ds/simpsons/Save1")&&opened.empty());
 assert(!select("sdmc:/other/art/l2z2.p3d")&&opened.empty());
 assert(!select("sdmc:/3ds/simpsons/art/l2z2.p3dx")&&opened.empty());
 exists=false;assert(select(world)==Error&&m_LastError==FileNotFound);
 assert(s_orderOpens[1][0]==1&&s_orderOpens[2][1]==1&&s_orderOpens[2][0]==0);
}
'''.replace('SELECTOR', source[begin:end])
with tempfile.TemporaryDirectory(prefix='stream-order-fixture-') as directory:
    path = Path(directory)
    (path / 'test.cpp').write_text(code)
    subprocess.run(['clang++', '-std=c++17', '-fsanitize=address,undefined',
                    str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: actual fixture selector isolates two read-only world files, preserves saves/writes/default paths and fails closed on a missing fixture.')
