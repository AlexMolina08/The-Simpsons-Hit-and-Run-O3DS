#!/usr/bin/env python3
"""Exercise actual benchmark argument parsing, including long-route duration."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3];text=(root/'code/main/nativebench.cpp').read_text();a=text.index('static char runIdentity[');b=text.index('struct PassiveFrame',a);fragment=text[a:b]
cache_source=(root/'libs/pure3d/pddi/pica/texture.cpp').read_text()
ca=cache_source.index('unsigned Mode(){return mode;}');cb=cache_source.index('void Start(unsigned level)',ca)
cache_policy=cache_source[ca:cb]
with tempfile.TemporaryDirectory(prefix='srr-bench-args-') as directory:
 p=Path(directory);cpp=p/'test.cpp'
 cpp.write_text('''#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <initializer_list>
#include <main/worldstressplan.h>
static WorldStress::Plan reloadPlan;
static bool worldResidencyOverride=false,irisCapture=false;
namespace rmt {struct Vector {float x,y,z;};}
static rmt::Vector roadAnchor={};static bool fixedRoadAnchor=false,residencyCpuProbe=false,missionNpcProbe=false;static unsigned suspensionProbe=0;
namespace WorldResidency {static bool lookupCache=true;void CacheLookup(bool enabled){lookupCache=enabled;}}

static float smallObjectCullSquared=4000.0f;static unsigned roadDirection=0,trafficLimit=0;static unsigned earlyWorldQueue=0;static bool stereoClip=false;static bool portraitActor=false,portraitNPC=false,portraitTraffic=false,portraitPlayerObserver=false,g_old3dsPreparedAudio=false,g_old3dsPreparedAudioShort=false;static char portraitModel[32]={},portraitActorModel[32]="male6";static float stereoTestSlider=-1,portraitDistance=1,portraitBearing=45;
static bool g_old3dsParkedSectionRemove=true;
static bool g_picaNativeExactWorldFallback=false,g_picaNativeBundleUnbuffered=false,g_picaNativeBundleCoalesced=false,g_picaVramCompact=false,g_picaLinearCompact=false;
static unsigned g_old3dsWorldOrderChoice=0,g_old3dsWorldResidencyChoice=0;
static int loaderCore=-1;
void radLoadSetCoreExperiment(int core){loaderCore=core;}
int radLoadCoreExperimentRequested(){return loaderCore;}
namespace picaNativeLevel {unsigned mode=0;bool attempted=false;
'''+cache_policy+'''
}
namespace picaNativeRead {bool enabled=false;}
namespace Old3dsPerformance { unsigned mode=1; }
namespace HandheldHudCadence { bool boostPickupAnimations=true; }
namespace Old3dsRecurring { bool requested=false,focusDropWindow=false;unsigned stationaryMs=0,windowStartMs=15000,windowEndMs=18000; }
static unsigned seconds=25,warmup=8,seed=1;static bool hold=false,reload=false,serviceProfile=false,afterRouteCapture=false,vehicleProfile=false,g_picaSkinPrepareYield=false,g_old3dsTrafficFacingOnly=false,g_old3dsAdpcmPrune=false,g_old3dsAdpcmVerify=false,g_old3dsAdpcmFast=false;
'''+fragment+'''
int main(){
 assert(ParseRunArgument("bench_mission_npc_probe=1") && missionNpcProbe);
 assert(!ParseRunArgument("bench_mission_npc_probe=1junk") && missionNpcProbe);
 assert(ParseRunArgument("bench_mission_npc_probe=0") && !missionNpcProbe);
 assert(ParseRunArgument("bench_suspension_probe=1") && suspensionProbe);
 assert(!ParseRunArgument("bench_suspension_probe=1junk") && suspensionProbe);
 assert(ParseRunArgument("bench_suspension_probe=2") && suspensionProbe==2);
 assert(ParseRunArgument("bench_suspension_probe=0") && !suspensionProbe);
 assert(ParseRunArgument("bench_loader_core=0") && loaderCore==0);
 assert(ParseRunArgument("bench_loader_core=1") && loaderCore==1);
 assert(!ParseRunArgument("bench_loader_core=2") && loaderCore==1);
 assert(!ParseRunArgument("bench_loader_core=1junk") && loaderCore==1);
#ifdef SRR2_OLD3DS_LEVEL_CACHE
 assert(!ParseRunArgument("bench_native_cache=1") && picaNativeLevel::Mode()==0);
 assert(ParseRunArgument("bench_loader_core=0"));
 assert(ParseRunArgument("bench_native_cache=1") && picaNativeLevel::Mode()==1 && loaderCore==0);
 assert(ParseRunArgument("bench_native_cache=2") && picaNativeLevel::Mode()==2);
 assert(!ParseRunArgument("bench_loader_core=1") && loaderCore==0);
 assert(!ParseRunArgument("bench_native_cache=3") && picaNativeLevel::Mode()==2);
 assert(!ParseRunArgument("bench_native_cache=2junk") && picaNativeLevel::Mode()==2);
 assert(ParseRunArgument("bench_native_cache=0") && picaNativeLevel::Mode()==0);
#else
 assert(!ParseRunArgument("bench_native_cache=1"));
#endif


 assert(ParseRunArgument("bench_world_residency=1") && g_old3dsWorldResidencyChoice==1);
 assert(ParseRunArgument("bench_world_residency=2") && g_old3dsWorldResidencyChoice==2);
 assert(ParseRunArgument("bench_world_residency=3") && g_old3dsWorldResidencyChoice==3);
 assert(ParseRunArgument("bench_world_residency=4") && g_old3dsWorldResidencyChoice==4);
 assert(!ParseRunArgument("bench_world_residency=5") && g_old3dsWorldResidencyChoice==4);
 assert(!ParseRunArgument("bench_world_residency=2junk") && g_old3dsWorldResidencyChoice==4);
 assert(ParseRunArgument("bench_world_residency=0") && g_old3dsWorldResidencyChoice==0);

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
 assert(ParseRunArgument("bench_reload_plan=4:7,1:2,7:7") && reloadPlan.count==3 && reloadPlan.levels[0]==3 && reloadPlan.missions[2]==6);
 assert(!ParseRunArgument("bench_reload_plan=4:7,8:1") && reloadPlan.count==3);
 assert(ParseRunArgument("bench_iris_capture=1") && irisCapture);
 assert(ParseRunArgument("bench_iris_capture=0") && !irisCapture);
 assert(!ParseRunArgument("bench_iris_capture=1junk"));
 assert(ParseRunArgument("bench_reload_active=1") && reloadPlan.activeMission);
 assert(ParseRunArgument("bench_reload_active=0") && !reloadPlan.activeMission);
 assert(!ParseRunArgument("bench_reload_active=1junk"));
 assert(ParseRunArgument("bench_reload_interior=1") && reloadPlan.interior);
 assert(ParseRunArgument("bench_reload_interior=0") && !reloadPlan.interior);
 assert(!ParseRunArgument("bench_reload_interior=1junk"));
 assert(ParseRunArgument("bench_reload_teleport=1") && reloadPlan.teleport);
 assert(ParseRunArgument("bench_reload_teleport=0") && !reloadPlan.teleport);
 assert(!ParseRunArgument("bench_reload_teleport=1junk"));
 assert(ParseRunArgument("bench_reload_reset=1") && reloadPlan.resetMission);
 assert(ParseRunArgument("bench_reload_reset=0") && !reloadPlan.resetMission);
 assert(!ParseRunArgument("bench_reload_reset=1junk"));
 assert(ParseRunArgument("bench_reload_dwell=3") && reloadPlan.dwellMs==3000);
 assert(!ParseRunArgument("bench_reload_dwell=2") && reloadPlan.dwellMs==3000);
 assert(!ParseRunArgument("bench_reload_dwell=3junk") && reloadPlan.dwellMs==3000);

 assert(ParseRunArgument("bench_seed=231171")&&seed==231171);
 assert(ParseRunArgument("bench_resident_lookup_cache=0")&&!WorldResidency::lookupCache);
 assert(ParseRunArgument("bench_resident_lookup_cache=1")&&WorldResidency::lookupCache);
 assert(!ParseRunArgument("bench_resident_lookup_cache=1junk")&&WorldResidency::lookupCache);
 assert(!ParseRunArgument("bench_resident_lookup_cache=2")&&WorldResidency::lookupCache);
 assert(ParseRunArgument("bench_residency_cpu_probe=1")&&residencyCpuProbe&&seed==231171);
 assert(ParseRunArgument("bench_residency_cpu_probe=0")&&!residencyCpuProbe);
 assert(!ParseRunArgument("bench_residency_cpu_probe=2"));
 assert(ParseRunArgument("bench_road_anchor=19.85,7.518,5")&&fixedRoadAnchor);
 assert(roadAnchor.x>19.84f&&roadAnchor.x<19.86f&&roadAnchor.z==5);
 for(const char* invalid:{"","1,2","1,2,3x","1,2,3,4","nan,1,2","1,inf,2","-10001,1,2"}){
  char argument[128];snprintf(argument,sizeof(argument),"bench_road_anchor=%s",invalid);
  assert(!ParseRunArgument(argument)&&roadAnchor.z==5&&roadAnchor.x>19.84f);
 }

 assert(ParseRunArgument("bench_trial=physical-test")&&!strcmp(runIdentity,"physical-test"));
 assert(ParseRunArgument("bench_world_order=1")&&g_old3dsWorldOrderChoice==1);
 assert(ParseRunArgument("bench_world_order=2")&&g_old3dsWorldOrderChoice==2);
 assert(!ParseRunArgument("bench_world_order=3")&&g_old3dsWorldOrderChoice==2);
 assert(ParseRunArgument("bench_world_order=0")&&g_old3dsWorldOrderChoice==0);
 assert(ParseRunArgument("bench_native_async=1")&&picaNativeRead::enabled);
 assert(ParseRunArgument("bench_native_async=0")&&!picaNativeRead::enabled);
 assert(!ParseRunArgument("bench_native_async=1junk")&&!picaNativeRead::enabled);
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
 for(const char* removed:{"bench_native_contiguous=1","bench_native_input_stage=1","bench_native_seek_lend=1","bench_particle_trig_cache=1","bench_billboard_batch=1","bench_billboard_compact=1"})assert(!ParseRunArgument(removed));
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
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
 assert(ParseRunArgument("bench_vram_compact=1")&&g_picaVramCompact);
 assert(ParseRunArgument("bench_vram_compact=0")&&!g_picaVramCompact);
 assert(ParseRunArgument("bench_linear_compact=1")&&g_picaLinearCompact);
 assert(!ParseRunArgument("bench_linear_compact=2")&&g_picaLinearCompact);
 assert(!ParseRunArgument("bench_linear_compact=1junk")&&g_picaLinearCompact);
 assert(ParseRunArgument("bench_linear_compact=0")&&!g_picaLinearCompact);

#endif
 assert(ParseRunArgument("bench_small_distance=100")&&smallObjectCullSquared==10000);
 assert(!ParseRunArgument("bench_small_distance=nan")&&smallObjectCullSquared==10000);
 assert(ParseRunArgument("bench_road_direction=west")&&roadDirection==3);
 assert(ParseRunArgument("bench_road_direction=forward")&&roadDirection==1);
 assert(ParseRunArgument("bench_road_direction=reverse")&&roadDirection==2);
 assert(!ParseRunArgument("bench_road_direction=bad")&&roadDirection==2);
 for(unsigned limit=1;limit<=5;++limit){char argument[64];snprintf(argument,sizeof(argument),"bench_traffic_limit=%u",limit);assert(ParseRunArgument(argument)&&trafficLimit==limit);}
 for(const char* invalid:{"bench_traffic_limit=0","bench_traffic_limit=6","bench_traffic_limit=nan","bench_traffic_limit=3x"})assert(!ParseRunArgument(invalid)&&trafficLimit==5);
#if defined(SRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE) && defined(SRR2_OLD3DS_NATIVE_ETC1)
 assert(ParseRunArgument("bench_parked_section_remove=0")&&!g_old3dsParkedSectionRemove);
 assert(ParseRunArgument("bench_parked_section_remove=1")&&g_old3dsParkedSectionRemove);
 assert(!ParseRunArgument("bench_parked_section_remove=2"));
 assert(ParseRunArgument("bench_native_coalesced=1")&&g_picaNativeBundleCoalesced);
 assert(ParseRunArgument("bench_native_coalesced=0")&&!g_picaNativeBundleCoalesced);
 assert(!ParseRunArgument("bench_native_coalesced=2"));
 assert(!ParseRunArgument("bench_native_coalesced=1junk"));
 assert(ParseRunArgument("bench_native_unbuffered=1")&&g_picaNativeBundleUnbuffered);
 assert(ParseRunArgument("bench_native_unbuffered=0")&&!g_picaNativeBundleUnbuffered);
 assert(ParseRunArgument("bench_exact_world_cache=1")&&g_picaNativeExactWorldFallback);
 assert(ParseRunArgument("bench_exact_world_cache=0")&&!g_picaNativeExactWorldFallback);
#endif
 assert(ParseRunArgument("bench_early_world_queue=1")&&earlyWorldQueue);
 assert(ParseRunArgument("bench_early_world_queue=2")&&earlyWorldQueue==2);
 assert(!ParseRunArgument("bench_early_world_queue=3")&&earlyWorldQueue==2);
 assert(ParseRunArgument("bench_early_world_queue=0")&&!earlyWorldQueue);
 assert(ParseRunArgument("bench_stereo_clip=1")&&stereoClip);
 assert(ParseRunArgument("bench_stereo_clip=0")&&!stereoClip);
 assert(ParseRunArgument("bench_recurring=1")&&Old3dsRecurring::requested);
 assert(ParseRunArgument("bench_stationary_ms=6000")&&Old3dsRecurring::stationaryMs==6000);
}
''')
 for enabled in (False,True):
  for cache_enabled in (False,True):
   subprocess.run(['c++','-std=c++17','-I'+str(root/'code'),'-DSRR2_OLD3DS_VRAM_TEXTURES','-DSRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE','-DSRR2_OLD3DS_NATIVE_ETC1','-DSRR2_OLD3DS_PREPARED_AUDIO','-DSRR2_OLD3DS_GPU_SKIN_SUPPORT','-DSRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE','-DSRR2_OLD3DS_BOTTOM_MAP_15HZ',*(['-DSRR2_OLD3DS_RECURRING_PROFILE'] if enabled else []),*(['-DSRR2_OLD3DS_LEVEL_CACHE'] if cache_enabled else []),'-fsanitize=address,undefined',str(cpp),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
print('PASS: production argument parsing with recurring profiling compiled on and off, bounded window validation and unchanged defaults.')
