#!/usr/bin/env python3
"""Test production ring/report code, including dump failure and overflow."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
diag=(root/'code/main/old3dsdiag.h').read_text()
def extract(start,end): return diag[diag.index(start):diag.index(end,diag.index(start))]
prelude=r'''
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
using u64=unsigned long long;
const u64 SYSCLOCK_ARM11=1000;
static u64 tick=1;
u64 svcGetSystemTick(){return tick+=10;}
static bool fail=false;
static std::string written;
int FakePrintf(const char* format,...) {
 char text[8192];va_list args;va_start(args,format);int n=vsnprintf(text,sizeof(text),format,args);va_end(args);
 written+=text;return n;
}
int FakeFlush(FILE*) {return fail?-1:0;}
int FakeError(FILE*) {return fail?1:0;}
void FakeClear(FILE*) {}
#define printf FakePrintf
#define fflush FakeFlush
#define ferror FakeError
#define clearerr FakeClear
'''
structs=extract('enum Old3dsCategory','extern "C" Old3dsEntityStats')+extract('struct Old3dsAnimStats','extern "C" Old3dsAnimStats')
code=prelude+structs+'\n#include <main/old3dsreport.h>\n'+r'''
int main() {
 Old3dsRing<unsigned,3> ring;
 for(unsigned i=0;i<10;++i) ring.Push()=i;
 assert(ring.Size()==3 && ring.Overwritten()==7);
 for(unsigned i=0;i<3;++i) assert(ring.At(i)==i+7);
 ring.Clear(); assert(!ring.Size() && !ring.Overwritten());
 for(unsigned i=0;i<70;++i) {Old3dsReport& r=s_old3dsReports.Push();r=Old3dsReport{};r.mFrameCount=i;r.profileFrames=1;}
 assert(s_old3dsReports.Size()==64 && s_old3dsReports.Overwritten()==6);
 fail=true; Old3dsDumpReports("failure-test"); assert(s_old3dsReports.Size()==64);
 fail=false; written.clear(); Old3dsDumpReports("retry-test"); assert(!s_old3dsReports.Size());
 assert(written.find("OLD3DS_FRAME context=0 frame=6 ")!=std::string::npos);
 assert(written.find("OLD3DS_FRAME context=0 frame=69 ")!=std::string::npos);
 assert(written.find("OLD3DS_DUMP ")!=std::string::npos);
 assert(written.find("ok=1")!=std::string::npos);
 // Unsigned clock arithmetic preserves real work across millisecond wrap.
 unsigned previous=0xfffffff0u, dumpStart=previous+15u, dumpEnd=dumpStart+250u;
 previous+=dumpEnd-dumpStart;
 assert((dumpEnd+5u)-previous==20u);
 std::fprintf(stderr,"Ring overflow/order, 64 raw snapshots, failed-dump retention, retry, report fields and clock-wrap tests passed; ring bytes=%zu\n",sizeof(s_old3dsReports));
}
'''
with tempfile.TemporaryDirectory(prefix='srr-telemetry-test-') as directory:
 temp=Path(directory);cpp=temp/'test.cpp';binary=temp/'test';cpp.write_text(code)
 subprocess.run(['clang++','-std=c++11','-O2','-fsanitize=address,undefined','-I'+str(root/'code'),str(cpp),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
game=(root/'code/main/game.cpp').read_text()
a=game.index('const u64 captureStart'); b=game.index('lastPresented = presented;',a)
assert 'printf(' not in game[a:b] and 'fflush(' not in game[a:b]
print('Periodic capture path contains no formatting or flush calls.')
