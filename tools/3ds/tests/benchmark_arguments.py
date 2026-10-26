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
#include <cmath>
#include <initializer_list>
static bool portraitActor=false,portraitNPC=false,portraitTraffic=false,portraitPlayerObserver=false,g_old3dsPreparedAudio=false,g_old3dsPreparedAudioShort=false;static char portraitModel[32]={},portraitActorModel[32]="male6";static float stereoTestSlider=-1,portraitDistance=1,portraitBearing=45;
namespace Old3dsPerformance { unsigned mode=1; }
namespace radNativeSeek { bool enabled=false,stageInput=false; }
namespace HandheldHudCadence { bool boostPickupAnimations=true; }
namespace Old3dsRecurring { bool requested=false,focusDropWindow=false;unsigned stationaryMs=0,windowStartMs=15000,windowEndMs=18000; }
static unsigned seconds=25,warmup=8,seed=1;static bool hold=false,reload=false,serviceProfile=false,afterRouteCapture=false,vehicleProfile=false,g_picaSkinPrepareYield=false,g_picaNativeContiguous=false,g_old3dsTrafficFacingOnly=false,g_old3dsAdpcmPrune=false,g_old3dsAdpcmVerify=false,g_old3dsAdpcmFast=false;
'''+fragment+'''
int main(){
 assert(ParseRunArgument("bench_quality=0")&&Old3dsPerformance::mode==0);
 assert(ParseRunArgument("bench_quality=2")&&Old3dsPerformance::mode==2);
 assert(ParseRunArgument("bench_quality=7")&&Old3dsPerformance::mode==7);
 assert(ParseRunArgument("bench_quality=6")&&Old3dsPerformance::mode==6);
 assert(ParseRunArgument("bench_quality=5")&&Old3dsPerformance::mode==5);
 assert(ParseRunArgument("bench_quality=4")&&Old3dsPerformance::mode==4);
 assert(ParseRunArgument("bench_quality=3")&&Old3dsPerformance::mode==3);
 assert(ParseRunArgument("bench_quality=1")&&Old3dsPerformance::mode==1);
 assert(!ParseRunArgument("bench_quality_wrong=0")&&Old3dsPerformance::mode==1);
 assert(ParseRunArgument("bench_seconds=60")&&seconds==60);
 assert(ParseRunArgument("bench_warmup=3")&&warmup==3);
 assert(ParseRunArgument("bench_hold=1")&&hold);
 assert(ParseRunArgument("bench_reload=1")&&reload);
 assert(ParseRunArgument("bench_seed=231171")&&seed==231171);
 assert(ParseRunArgument("bench_trial=physical-test")&&!strcmp(runIdentity,"physical-test"));
#ifdef SRR2_OLD3DS_RECURRING_PROFILE
 assert(Old3dsRecurring::windowStartMs==15000&&Old3dsRecurring::windowEndMs==18000);
 assert(ParseRunArgument("bench_recurring_window=43000,47000"));
 assert(Old3dsRecurring::windowStartMs==43000&&Old3dsRecurring::windowEndMs==47000);
 for(const char* invalid:{"",",47000","43000,","43000,43000","47000,43000","0,60001","-1,47000","43000,-1","43000,47000x","43000,47000,48000","999999999999999999999,60000"}){
  char arg[160];snprintf(arg,sizeof(arg),"bench_recurring_window=%s",invalid);
  assert(!ParseRunArgument(arg));
  assert(Old3dsRecurring::windowStartMs==43000&&Old3dsRecurring::windowEndMs==47000);
 }
 assert(ParseRunArgument("bench_recurring_window=0,60000"));
 assert(Old3dsRecurring::windowStartMs==0&&Old3dsRecurring::windowEndMs==60000);
#else
 assert(!ParseRunArgument("bench_recurring_window=43000,47000"));
 assert(Old3dsRecurring::windowStartMs==15000&&Old3dsRecurring::windowEndMs==18000);
#endif
 assert(ParseRunArgument("bench_service_profile=1")&&serviceProfile);
 assert(ParseRunArgument("bench_service_profile=0")&&!serviceProfile);
 assert(ParseRunArgument("bench_after_route_capture=1")&&afterRouteCapture);
 assert(ParseRunArgument("bench_after_route_capture=0")&&!afterRouteCapture);
 assert(ParseRunArgument("bench_skin_prepare_yield=1")&&g_picaSkinPrepareYield);
 assert(ParseRunArgument("bench_skin_prepare_yield=0")&&!g_picaSkinPrepareYield);
 assert(ParseRunArgument("bench_native_contiguous=1")&&g_picaNativeContiguous);
 assert(ParseRunArgument("bench_native_contiguous=0")&&!g_picaNativeContiguous);
 assert(ParseRunArgument("bench_native_input_stage=1")&&radNativeSeek::stageInput);
 assert(ParseRunArgument("bench_native_input_stage=0")&&!radNativeSeek::stageInput);
 assert(ParseRunArgument("bench_native_seek_lend=1")&&radNativeSeek::enabled);
 assert(ParseRunArgument("bench_native_seek_lend=0")&&!radNativeSeek::enabled);
 assert(ParseRunArgument("bench_bottom_animation_boost=0")&&!HandheldHudCadence::boostPickupAnimations);
 assert(ParseRunArgument("bench_bottom_animation_boost=1")&&HandheldHudCadence::boostPickupAnimations);
 assert(ParseRunArgument("bench_portrait_actor=fem1")&&portraitActor&&!strcmp(portraitActorModel,"fem1"));
 assert(!ParseRunArgument("bench_portrait_actor=../bad")&&!strcmp(portraitActorModel,"fem1"));
 assert(ParseRunArgument("bench_portrait_traffic=1")&&portraitTraffic);
 assert(ParseRunArgument("bench_portrait_model=taxiA")&&!strcmp(portraitModel,"taxiA"));
 assert(!ParseRunArgument("bench_portrait_model=../bad")&&!strcmp(portraitModel,"taxiA"));
 assert(ParseRunArgument("bench_portrait_observer=player")&&portraitPlayerObserver);
 assert(ParseRunArgument("bench_portrait_distance=6")&&portraitDistance==6);
 assert(ParseRunArgument("bench_portrait_distance=nan")&&portraitDistance==6);
 assert(ParseRunArgument("bench_portrait_bearing=180")&&portraitBearing==180);
 assert(ParseRunArgument("bench_portrait_bearing=181")&&portraitBearing==180);
 assert(ParseRunArgument("bench_prepared_audio=1")&&g_old3dsPreparedAudio);
 assert(ParseRunArgument("bench_prepared_audio=0")&&!g_old3dsPreparedAudio);
 assert(ParseRunArgument("bench_prepared_audio_short=1")&&g_old3dsPreparedAudioShort);
 assert(ParseRunArgument("bench_prepared_audio_short=0")&&!g_old3dsPreparedAudioShort);
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
 for enabled in (False,True):
  subprocess.run(['c++','-std=c++17','-DSRR2_OLD3DS_PREPARED_AUDIO','-DSRR2_OLD3DS_GPU_SKIN_SUPPORT','-DSRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE','-DSRR2_OLD3DS_BOTTOM_MAP_15HZ',*(['-DSRR2_OLD3DS_RECURRING_PROFILE'] if enabled else []),'-fsanitize=address,undefined',str(cpp),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
print('PASS: production argument parsing with recurring profiling compiled on and off, bounded window validation and unchanged defaults.')
