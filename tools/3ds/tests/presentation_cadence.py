#!/usr/bin/env python3
"""Exercise production swap/VBlank hooks without replacing SDK callbacks."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='srr-cadence-') as directory:
 p=Path(directory);(p/'main').mkdir()
 (p/'main/nativebench.h').write_text('namespace NativeBench {extern bool collecting;}\n')
 (p/'3ds.h').write_text('''#include <cstdint>
using u64=unsigned long long;using ThreadFunc=void(*)(void*);
enum GSPGPU_Event {GSPGPU_EVENT_VBlank0,GSPGPU_EVENT_VBlank1};
enum gfxScreen_t {GFX_TOP,GFX_BOTTOM};
const unsigned GSP_SCREEN_TOP=0,SYSCLOCK_ARM11=1000000;
u64 svcGetSystemTick();bool gspIsPresentPending(unsigned);void gspWaitForVBlank();
''')
 source=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "@SOURCE@"
namespace NativeBench {bool collecting=false;}
static ThreadFunc installed;static void* argument;static bool waiting;static unsigned priorCalls,swaps;static u64 clockTick;
u64 svcGetSystemTick(){return ++clockTick;}
bool gspIsPresentPending(unsigned){return waiting;}
void gspWaitForVBlank(){waiting=false;if(installed)installed(argument);}
extern "C" void __real_gspSetEventCallback(GSPGPU_Event,ThreadFunc cb,void* data,bool){installed=cb;argument=data;}
extern "C" void __real_gfxScreenSwapBuffers(gfxScreen_t,bool){++swaps;waiting=true;}
void prior(void* data){assert(data==&priorCalls);++priorCalls;}
std::string dump(){FILE* f=tmpfile();Old3dsCadence::Dump(f);fflush(f);rewind(f);std::string out;char b[1024];while(fgets(b,sizeof b,f))out+=b;fclose(f);return out;}
int main(){
 __wrap_gspSetEventCallback(GSPGPU_EVENT_VBlank0,prior,&priorCalls,false);
 Old3dsCadence::enabled=true;NativeBench::collecting=true;
 Old3dsCadence::Submit(7);__wrap_gfxScreenSwapBuffers(GFX_TOP,false);
 installed(argument);assert(priorCalls==1);assert(Old3dsCadence::observations[7].identity==0);
 // GSP has not consumed the request: a VBlank callback alone is insufficient.
 waiting=false;installed(argument);assert(priorCalls==2);
 assert(Old3dsCadence::observations[7].identity==7);
 Old3dsCadence::Submit(8);__wrap_gfxScreenSwapBuffers(GFX_BOTTOM,false);
 waiting=false;installed(argument);assert(Old3dsCadence::observations[8].identity==0);
 Old3dsCadence::Submit(9);__wrap_gfxScreenSwapBuffers(GFX_TOP,false);
 Old3dsCadence::Submit(10);__wrap_gfxScreenSwapBuffers(GFX_TOP,false);
 NativeBench::collecting=false;auto text=dump();
 assert(text.find("replaced=1")!=std::string::npos);
 assert(text.find("submit_id=7 observed=1")!=std::string::npos);
 assert(text.find("submit_id=9 observed=0")!=std::string::npos);
 assert(text.find("submit_id=10 observed=1")!=std::string::npos);
 Old3dsCadence::Submit(11);__wrap_gfxScreenSwapBuffers(GFX_TOP,false);waiting=false;installed(argument);
 assert(Old3dsCadence::observations[11].identity==0);
 Old3dsCadence::enabled=false;assert(dump().empty());assert(swaps==5);
 __wrap_gspSetEventCallback(GSPGPU_EVENT_VBlank0,nullptr,nullptr,false);assert(!installed);
 puts("PASS: prior callbacks and swaps preserved; pending GSP requests excluded, bottom-screen and uncaptured swaps excluded, replaced requests and drain recorded.");
}
'''.replace('@SOURCE@',str(root/'code/main/old3dscadence.cpp'))
 (p/'test.cpp').write_text(source)
 subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-DSRR2_OLD3DS_NATIVE_BENCH','-I'+str(p),'-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
