#!/usr/bin/env python3
"""Verify production drive-root construction on reused, nonzero heap storage."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "libs/radcore/src/radfile/sdl/sdldrive.cpp").read_text()
start = source.index("radSdlDrive::radSdlDrive(")
opening = source.index("{", start)
depth = 1
end = opening + 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
constructor = source[start:end]
fixture = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <new>
#define SDL_MAJOR_VERSION 2
#define SRR2_OLD3DS_NATIVE_BENCH
#define rAssert(x) assert(x)
using radMemoryAllocator=int;
void* operator new(std::size_t bytes,int){return ::operator new(bytes);}
void operator delete(void* pointer,int){::operator delete(pointer);}
const unsigned radFileDrivenameMax=16,radFileFilenameMax=256;
enum {radDriveWriteable=1,radDriveFile=2,radDriveEnumerable=4,radDriveDirectory=8};
struct radDrive{};
struct Mutex{};
void radThreadCreateMutex(Mutex** p,int){static Mutex mutex;*p=&mutex;}
struct radDriveThread{radDriveThread(Mutex*,int){}};
void radGetDefaultDrive(char* p){std::strcpy(p,"SDMC:");}
void SDL_strupr(char* p){for(;*p;++p)*p=std::toupper(static_cast<unsigned char>(*p));}
void SDL_strlwr(char* p){for(;*p;++p)*p=std::tolower(static_cast<unsigned char>(*p));}
unsigned cwdCalls=0;
char* getcwd(char* p,unsigned n){++cwdCalls;std::strncpy(p,"sdmc:/3ds/simpsons",n);return p;}
struct radSdlDrive:radDrive {
 unsigned m_OpenFiles;Mutex* m_pMutex;radDriveThread* m_pDriveThread;
 char m_DriveName[radFileDrivenameMax+1],m_DrivePath[radFileFilenameMax+1];
 unsigned m_Capabilities;
 radSdlDrive(const char*,radMemoryAllocator);
 ~radSdlDrive(){delete m_pDriveThread;}
};
'''
main = r'''
int main(){
 alignas(radSdlDrive) unsigned char storage[sizeof(radSdlDrive)];
 for(unsigned fill:{0u,0xa5u,0xcdu,0xffu}){
  std::memset(storage,fill,sizeof(storage));
  unsigned before=cwdCalls;
  radSdlDrive* drive=new(storage)radSdlDrive("SDMC:",0);
  if(cwdCalls!=before+1)return 77;
  assert(!std::strncmp(drive->m_DrivePath,"sdmc:/3ds/simpsons/",19));
  drive->~radSdlDrive();
 }
 std::memset(storage,0xa5,sizeof(storage));
 unsigned before=cwdCalls;
 radSdlDrive* drive=new(storage)radSdlDrive("SDMC:/OTHER/",0);
 assert(cwdCalls==before);
 assert(!std::strcmp(drive->m_DrivePath,"sdmc:/other/"));
 drive->~radSdlDrive();
}
'''
with tempfile.TemporaryDirectory(prefix="srr-drive-root-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text("#include <initializer_list>\n" + fixture + constructor + main)
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", str(path / "test.cpp"), "-o", str(path / "test"),
    ], check=True)
    subprocess.run([str(path / "test")], check=True)
    # Removing the initialization must reproduce the reused-storage failure.
    old = constructor.replace("    m_DrivePath[0] = '\\0';\n", "", 1)
    assert old != constructor
    (path / "old.cpp").write_text("#include <initializer_list>\n" +
        fixture.replace("#define SRR2_OLD3DS_NATIVE_BENCH", "") + old + main)
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", str(path / "old.cpp"), "-o", str(path / "old"),
    ], check=True)
    assert subprocess.run([str(path / "old")]).returncode == 77
print("PASS: default drive resolves CWD on zeroed/reused storage; explicit drive paths remain intact.")
