#!/usr/bin/env python3
"""Compile the actual private argument parser and reject unbounded candidates."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/main/worldstreaming.cpp').read_text()
fragment = source[source.index('bool predictive=false'):source.index('struct ZoneEvent')]
code = '''
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <initializer_list>
#include <cassert>
#include "code/main/streamingpolicy.h"
#define SRR2_OLD3DS_NATIVE_BENCH
#include "code/main/worldstreaming.h"
namespace NativeBench {bool collecting=false;}
class Avatar;
namespace WorldStreaming {
''' + fragment + r'''
}
int main(){
 using namespace WorldStreaming;
 // NativeBench::End clears collecting before it records population counters.
 NativeBench::collecting=false;passiveActive=false;PedSlots(4,3);
 assert(pedSlotSamples==1 && pedReservedTotal==4 && pedAwaitingTotal==3 && pedReservedMaximum==4);
 assert(!predictive&&!audit&&!manual&&!pedestrianLimit&&cruiseSpeed==18.f);
 assert(idleLoader);assert(ParseArgument("bench_idle_loader=0")&&!idleLoader);
 assert(ParseArgument("bench_idle_loader=1")&&idleLoader);assert(!ParseArgument("bench_idle_loader=2"));
 assert(!pedestrianReuse);assert(ParseArgument("bench_pedestrian_reuse=1")&&pedestrianReuse);
 assert(!ParseArgument("bench_pedestrian_reuse=2"));assert(ParseArgument("bench_pedestrian_reuse=0")&&!pedestrianReuse);
 assert(!worldRetention);assert(ParseArgument("bench_world_retention=1")&&worldRetention);
 assert(!ParseArgument("bench_world_retention=2"));assert(ParseArgument("bench_world_retention=0")&&!worldRetention);
 assert(ParseArgument("bench_stream_fly=1")&&fly);
 assert(ParseArgument("bench_fly_speed=60")&&flightSpeed==60.f);
 assert(!ParseArgument("bench_fly_speed=nan")&&!ParseArgument("bench_fly_speed=81")&&!ParseArgument("bench_fly_speed="));
 assert(ParseArgument("bench_predictive_streaming=1")&&predictive);
 assert(ParseArgument("bench_stream_audit=1")&&audit);
 assert(ParseArgument("bench_manual=1")&&manual);
 assert(ParseArgument("bench_pedestrians=6")&&pedestrianLimit==6);
 assert(ParseArgument("bench_traversal=3")&&traversal==3);
 assert(ParseArgument("bench_camera_sweep=1")&&cameraSweep==1);
 assert(ParseArgument("bench_cruise_speed=28")&&cruiseSpeed==28.f);
 for(const char* bad:{"bench_predictive_streaming=2","bench_predictive_streaming=1junk",
  "bench_pedestrians=8","bench_pedestrians=-1","bench_pedestrians=6junk",
  "bench_traversal=4","bench_cruise_speed=nan","bench_cruise_speed=inf",
  "bench_cruise_speed=999","bench_cruise_speed=28junk"})assert(!ParseArgument(bad));
 assert(pedestrianLimit==6&&traversal==3&&cruiseSpeed==28.f);
 assert(ScanDue(0));assert(!ScanDue(99));assert(ScanDue(100));
}
'''
with tempfile.TemporaryDirectory(prefix='streaming-arguments-') as td:
    p = Path(td)
    (p / 'test.cpp').write_text(code)
    subprocess.run(['c++', '-std=c++17', '-fsanitize=address,undefined', '-I', str(root),
                    str(p / 'test.cpp'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('PASS: production streaming, traversal, manual-input and population arguments; rejected invalid bounds and non-finite speed')
