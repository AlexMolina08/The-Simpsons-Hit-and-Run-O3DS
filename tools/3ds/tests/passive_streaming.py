#!/usr/bin/env python3
"""Prove passive recording cannot select a route, drive or replace game timing."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/main/nativebench.cpp').read_text()
start = source.index('int Input(bool axis,int id,int original)')
end = source.index('\n}\nextern "C" int nativeBenchInput', start)
input_function = source[start:end]
start = source.index('unsigned Seed(unsigned fallback)')
seed_function = source[start:source.index('\n', start)]
start = source.index(' if(WorldStreaming::passive) {', source.index('void Init('))
end = source.index('\n FILE* f=fopen("native-automation.txt"', start)
init_branch = source[start:end]
assert source.index('if(!PrepareSaveFixtures()', source.index('void Init(')) < start
assert 'CommandLineOptions' not in init_branch and 'BuildRoadRoute' not in init_branch
assert '!strncmp(argv[i],"bench_trial=",12) ||' in init_branch
assert '!strncmp(argv[i],"bench_world_residency=",22)' in init_branch
begin = source[source.index('unsigned Begin(unsigned elapsed,int context){'):]
assert begin.index('if(WorldStreaming::passive)') < begin.index('trafficVisible=0')
early_begin = begin[begin.index(' if(WorldStreaming::passive)'):begin.index('\n trafficVisible=0')]

code = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <random>
using u64=unsigned long long;
static bool enabled=true,done=false,placed=true,portrait=false,roadRoute=true,serviceProfile=false;
static unsigned warmMs=99999,warmup=8,routeMs=12345,seed=42,passiveCalls=0;
static int roadSteer=32767,roadGas=1,roadBrake=1;static float roadDistance=100.f;
static char runIdentity[64]="unit-passive";
static u64 passiveStart=0,passivePrevious=0;
static constexpr unsigned PassiveCapacity=2048;
struct PassiveFrame{unsigned value;};static PassiveFrame* passiveFrames=nullptr;
static u64 svcGetSystemTick(){return 1234;}
namespace WorldResidency {void Init(){}}
namespace WorldStreaming {
 bool passive=true,audit=false,predictive=true,manual=false,cameraSweep=true,idleLoader=true;
 static unsigned init=0;void Init(){++init;}
}
namespace Old3dsRecurring {unsigned stationaryMs=1000;}
enum {SDL_CONTROLLER_AXIS_LEFTX=1,SDL_CONTROLLER_AXIS_RIGHTX=2,
 SDL_CONTROLLER_BUTTON_DPAD_DOWN=3,SDL_CONTROLLER_BUTTON_A=4,SDL_CONTROLLER_BUTTON_B=5,SDL_CONTROLLER_BUTTON_Y=6};
static std::vector<std::string> parsed;
static bool ParseRunArgument(const char* text){parsed.push_back(text);return true;}
static void PassiveBegin(int){++passiveCalls;}
''' + input_function + '\n' + seed_function + '\nvoid InitPassive(int argc,char** argv){\n' + init_branch + '\n}\n'
code += 'unsigned BeginPassive(unsigned elapsed,int context){\n' + early_begin + '\nassert(false);return 0;\n}\n'
code += r'''
int main(){
 char a[]="game",b[]="bench_passive=1",c[]="bench_trial=incident",d[]="bench_quality=0",e[]="bench_route=road",f[]="bench_idle_loader=0",g[]="bench_world_residency=2";
 char h[]="bench_vram_compact=1",j[]="bench_linear_compact=1";
 char* argv[]={a,b,c,d,e,f,g,h,j};InitPassive(9,argv);
 assert(!enabled&&serviceProfile&&WorldStreaming::audit&&!WorldStreaming::predictive&&!WorldStreaming::idleLoader);
 assert(WorldStreaming::init==1&&passiveFrames&&passiveStart==1234&&passivePrevious==1234);
 assert((parsed==std::vector<std::string>{"bench_trial=incident","bench_world_residency=2","bench_vram_compact=1","bench_linear_compact=1"}));
 std::mt19937 random(8142);
 for(unsigned i=0;i<10000;++i){
  const int original=int(random()%65536)-32768;
  assert(Input(bool(i%2),int(i%32),original)==original);
  unsigned fallback=random();assert(Seed(fallback)==fallback);
 }
 for(unsigned dt:{0u,16u,33u,67u,150u})assert(BeginPassive(dt,9)==dt);
 assert(passiveCalls==5);free(passiveFrames);
}
'''
with tempfile.TemporaryDirectory(prefix='passive-streaming-') as td:
    p = Path(td)
    (p / 'test.cpp').write_text(code)
    subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined', str(p / 'test.cpp'),
                    '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('PASS: passive init bypasses automation/config/placement; 10,000 original inputs and RNG seeds pass through; elapsed time unchanged; original-save fixture path retained')
