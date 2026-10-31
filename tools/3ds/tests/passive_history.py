#!/usr/bin/env python3
"""A long pause must not erase the last driving seconds from the incident."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/main/nativebench.cpp').read_text()
dump = source[source.index('static void DumpPassive()'):source.index('void Init(int argc')]
record = source[source.index('struct PassiveFrame {'):source.index('static PassiveFrame*')]
code = r'''
#include <cstdio>
#include <cassert>
#include <cstring>
#define SRR2_OLD3DS_BUILD_ID "history-test"
constexpr double SYSCLOCK_ARM11=268123856.0;
namespace WorldStreaming {bool passiveActive=true;void Dump(FILE*,const char*){}}
void picaDumpStereoDiagnostics(FILE*){}
struct Vehicle {const char* GetName(){return "test";}};struct Avatar {bool IsInCar(){return false;}Vehicle* GetVehicle(){return nullptr;}};struct Avatars {Avatar* GetAvatarForPlayer(int){return nullptr;}};static Avatars avatars;static Avatars* GetAvatarManager(){return &avatars;}
static const char runIdentity[]="history-test";
static bool passiveMarked=true;
static unsigned passiveMs=209620,passiveLastGameplay=178428,passiveContext=10;
static unsigned passiveCount=3,saveFixture=0,fixtureBytes[4]={},fixtureHash[4]={};
''' + record + r'''
static PassiveFrame storage[PassiveCapacity];static PassiveFrame* passiveFrames=storage;
''' + dump + r'''
int main(){
 storage[0].ms=150000;storage[1].ms=159000;storage[2].ms=178000;
 DumpPassive();assert(!WorldStreaming::passiveActive);
 FILE* f=fopen("native-history-test.log","rb");assert(f);
 char report[8192]={};assert(fread(report,1,sizeof(report)-1,f)>0);fclose(f);
 assert(!strstr(report,"PASSIVE_FRAME ms=150000 "));
 assert(strstr(report,"PASSIVE_FRAME ms=159000 "));
 assert(strstr(report,"PASSIVE_FRAME ms=178000 "));
 assert(strstr(report,"pause_gap_ms=31192 marker_context=10 gameplay_frames_only=1"));
}
'''
with tempfile.TemporaryDirectory(prefix='passive-history-') as directory:
    p = Path(directory)
    (p / 'test.cpp').write_text(code)
    subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined',
                    str(p / 'test.cpp'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], cwd=p, check=True)
print('PASS: actual passive exporter retains preceding driving history after a 31-second pause and deactivates the observer before teardown.')
