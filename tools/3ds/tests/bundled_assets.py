#!/usr/bin/env python3
"""Exercise the engine's bundled read routing and writable SD compatibility."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[3]
source = (repo / 'libs/radcore/src/radfile/sdl/sdldrive.cpp').read_text()
start = source.index('#ifdef RAD_3DS\n    *pHandle = NULL;', source.index('radSdlDrive::OpenFile'))
end = source.index('\n#else', start)
body = source[start:end]
harness = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <string>
#include <vector>
#define RAD_3DS
const unsigned radFileFilenameMax=256;
enum {OpenExisting, OpenAlways, CreateAlways};
std::vector<std::string> calls;
bool bundledPresent, sdPresent;
std::string bundledOnly, lowercaseResult, resolvedResult;
bool srr2LowercaseDirectories(const char*,char* result,unsigned size) {
 if(lowercaseResult.empty() || lowercaseResult.size() >= size) return false;
 std::strcpy(result,lowercaseResult.c_str()); return true;
}
bool srr2ResolveCasePath(const char*,const char*,char* result,unsigned size) {
 if(resolvedResult.empty() || resolvedResult.size() >= size) return false;
 std::strcpy(result,resolvedResult.c_str()); return true;
}
void* SDL_RWFromFile(const char* path,const char*) {
 calls.push_back(path);
 bool rom=std::strncmp(path,"romfs:/",7)==0;
 return (rom?(bundledPresent && (bundledOnly.empty() || bundledOnly==path)):sdPresent)?reinterpret_cast<void*>(1):nullptr;
}
void run(const char* path,int flags,bool writeAccess,bool bundle,bool sd,
         const char* expected,unsigned count) {
 char fullName[257];std::strcpy(fullName,path);
 const char* createFlags=writeAccess?"rb+":"rb";
 void* handle=nullptr;void** pHandle=&handle;
 bundledPresent=bundle;sdPresent=sd;calls.clear();
 BODY
 assert(calls.size()==count);
 assert(calls.back()==expected);
 if(handle && calls.back().find("romfs:/")==0)assert(std::string(fullName)==expected);
}
int main(){
 run("sdmc:/3ds/simpsons/art/a.p3d",OpenExisting,false,true,true,"romfs:/art/a.p3d",1);
 run("sdmc:/3ds/simpsons/scripts.rcf",OpenExisting,false,false,true,"sdmc:/3ds/simpsons/scripts.rcf",2);
 run("sdmc:/3ds/simpsons/save.dat",OpenExisting,true,true,true,"sdmc:/3ds/simpsons/save.dat",1);
 run("sdmc:/3ds/simpsons/save.dat",CreateAlways,true,true,true,"sdmc:/3ds/simpsons/save.dat",1);
 run("sdmc:/3ds/other/game.dat",OpenExisting,false,true,true,"sdmc:/3ds/other/game.dat",1);
 run("sdmc:/3ds/simpsons/art/missing.p3d",OpenExisting,false,false,false,"sdmc:/3ds/simpsons/art/missing.p3d",2);
 bundledOnly=lowercaseResult="romfs:/art/A.p3d";
 run("sdmc:/3ds/simpsons/ART/A.p3d",OpenExisting,false,true,false,"romfs:/art/A.p3d",2);
 bundledOnly=resolvedResult="romfs:/Art/A.p3d";
 run("sdmc:/3ds/simpsons/ART/A.p3d",OpenExisting,false,true,false,"romfs:/Art/A.p3d",3);
}
'''.replace(' BODY',body)
with tempfile.TemporaryDirectory() as folder:
    folder=Path(folder)
    (folder/'test.cpp').write_text(harness)
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror',str(folder/'test.cpp'),'-o',str(folder/'test')],check=True)
    subprocess.run([str(folder/'test')],check=True)
print('PASS: bundled assets and case fallbacks resolve, SD-only homebrew remains supported, writes stay on SD, unrelated paths remain untouched.')
