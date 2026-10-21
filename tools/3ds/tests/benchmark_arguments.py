#!/usr/bin/env python3
"""Exercise actual benchmark argument parsing, including long-route duration."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3];text=(root/'code/main/nativebench.cpp').read_text();a=text.index('static char runIdentity[');b=text.index('void Init(',a);fragment=text[a:b]
with tempfile.TemporaryDirectory(prefix='srr-bench-args-') as directory:
 p=Path(directory);cpp=p/'test.cpp'
 cpp.write_text('''#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
namespace Old3dsRecurring { bool requested=false;unsigned stationaryMs=0; }
static unsigned seconds=25,warmup=8,seed=1;static bool hold=false,reload=false,serviceProfile=false,vehicleProfile=false,g_old3dsTrafficFacingOnly=false,g_old3dsAdpcmPrune=false,g_old3dsAdpcmVerify=false,g_old3dsAdpcmFast=false;
'''+fragment+'''
int main(){
 assert(ParseRunArgument("bench_seconds=60")&&seconds==60);
 assert(ParseRunArgument("bench_warmup=3")&&warmup==3);
 assert(ParseRunArgument("bench_hold=1")&&hold);
 assert(ParseRunArgument("bench_reload=1")&&reload);
 assert(ParseRunArgument("bench_seed=231171")&&seed==231171);
 assert(ParseRunArgument("bench_trial=physical-test")&&!strcmp(runIdentity,"physical-test"));
 assert(ParseRunArgument("bench_service_profile=1")&&serviceProfile);
 assert(ParseRunArgument("bench_service_profile=0")&&!serviceProfile);
 assert(ParseRunArgument("bench_adpcm_fast=1")&&g_old3dsAdpcmFast);
 assert(ParseRunArgument("bench_adpcm_fast=0")&&!g_old3dsAdpcmFast);
 assert(ParseRunArgument("bench_adpcm_prune=0")&&!g_old3dsAdpcmPrune);
 assert(ParseRunArgument("bench_adpcm_verify=1")&&g_old3dsAdpcmVerify);
 assert(ParseRunArgument("bench_traffic_facing=1")&&g_old3dsTrafficFacingOnly);
 assert(ParseRunArgument("bench_vehicle_profile=1")&&vehicleProfile);
 assert(!ParseRunArgument("bench_seconds_wrong=2")&&seconds==60);
 assert(!ParseRunArgument("bench_route=road"));
 assert(!ParseRunArgument("bench_candidate_flip=1"));
 assert(!ParseRunArgument("bench_candidate_ab=1"));
 assert(!ParseRunArgument("bench_candidate_mask=9"));
 assert(!ParseRunArgument("bench_emitter_clip=1"));
 assert(!ParseRunArgument("bench_emitter_clip=0"));
 assert(!ParseRunArgument("bench_particle_motion=1"));
 assert(!ParseRunArgument("bench_particle_motion=0"));
 assert(!ParseRunArgument("bench_particle_projection=1"));
 assert(!ParseRunArgument("bench_particle_projection=0"));
 assert(!ParseRunArgument("bench_fused_trig=1"));
 assert(!ParseRunArgument("bench_fused_trig=0"));
 assert(ParseRunArgument("bench_recurring=1")&&Old3dsRecurring::requested);
 assert(ParseRunArgument("bench_stationary_ms=6000")&&Old3dsRecurring::stationaryMs==6000);
}
''')
 subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined',str(cpp),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print('PASS: effective duration/warmup/hold/reload/seed/trial identity; unknown options remain unconsumed.')
