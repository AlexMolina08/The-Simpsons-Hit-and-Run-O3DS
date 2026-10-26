#include <main/graphics3ds.h>
#include <main/old3dsperformance.h>
#include <main/old3dsrecurring.h>
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#endif
#include <main/old3dsbottommap.h>
#include <main/old3dscadence.h>
#include <main/old3dsmemory.h>
#include <main/old3dspools.h>
#include <main/old3dsmainprofile.h>
#if defined(SRR2_OLD3DS_VEHICLE_PROFILE) || defined(SRR2_OLD3DS_NATIVE_BENCH)
#include <worldsim/redbrick/old3dsvehicleprofile.h>
#endif
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
#include <pddi/pica/nativecontext.h>
#endif
#include <main/nativebench.h>
#ifdef SRR2_OLD3DS_GEOMETRY_INPUT
#include <p3d/geometryinput.hpp>
#endif
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <main/commandlineoptions.h>
#include <contexts/context.h>
#include <loading/loadingmanager.h>
#include <worldsim/avatarmanager.h>
#include <worldsim/character/character.h>
#include <worldsim/redbrick/vehicle.h>
#include <mission/gameplaymanager.h>
#include <main/old3dsdiag.h>
#include <main/old3dsexperiment.h>
#include <citro3d.h>
#include <radfile.hpp>
#include <p3d/loadmanager.hpp>
#include <SDL.h>
#include <camera/supercammanager.h>
#include <camera/supercamcentral.h>
#include <camera/supercam.h>
#include <camera/staticcam.h>
#include <worldsim/character/charactertarget.h>
#ifdef SRR2_OLD3DS_DISTANCE_HAZE
#include <pddi/pica/distancehaze.hpp>
#endif
#ifdef SRR2_OLD3DS_CHARACTER_FORM
#include <pddi/pica/characterform.hpp>
#endif
#include <presentation/gui/guisystem.h>
#include <presentation/gui/ingame/guimanageringame.h>
#include <roads/road.h>
#include <roads/roadsegment.h>
#include <roads/intersection.h>
#include <render/IntersectManager/IntersectManager.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#ifdef SRR2_OLD3DS_SUBMIT_PROFILE
extern "C" void nativeBenchDumpSubmissionProfile(FILE*);
#endif
#ifdef SRR2_OLD3DS_COARSE_POOLS
namespace Old3dsPools {
unsigned family=0;
unsigned long long ticks[Count]={};
unsigned calls[Count]={};
static void* mainTls=nullptr;
static bool active=false;
static unsigned selectedFrames=0;
static int stage=Uncovered;
static u64 last=0;
static void Change(int next)
{
    const u64 now=svcGetSystemTick();
    ticks[stage]+=now-last;last=now;stage=next;
}
void Configure(unsigned value){family=value<=Streaming?value:0;mainTls=getThreadLocalStorage();}
void Begin(unsigned frame)
{
    active=family && frame%32==17;
    if(active){++selectedFrames;stage=Uncovered;last=svcGetSystemTick();}
}
void End(){if(active){Change(Uncovered);active=false;}}
bool InspectRecipe(){return getThreadLocalStorage()==mainTls && active && family==Recipe;}
int Select(unsigned group,unsigned next)
{
    if(getThreadLocalStorage()!=mainTls || !active || family!=group)return -1;
    const int previous=stage;
    // Backend/pose/fallback outside a composite is not recipe coverage.
    if(group==Recipe && previous==Uncovered && next!=Upstream)return -1;
    Change(next);++calls[next];return previous;
}
void Restore(int previous){if(previous>=0 && active)Change(previous);}
void Dump(FILE* f)
{
    static const char* names[]={"uncovered","areas","ground","pre_sim","vehicle_prep","character_prep",
        "actions","detect","solve","vehicles","characters","camera","dynamics","world","hud","bottom",
        "upstream_upper_bound","backend_excluded","pose_excluded","fallback_excluded","load_handoff","callbacks"};
    fprintf(f,"NATIVE_POOLS family=%u selected_frames=%u stride=32 disjoint=1 main_thread_only=1 recipe_is_upper_bound=1 loader_cpu_unattributed=1\n",family,selectedFrames);
    for(unsigned i=0;i<Count;++i)fprintf(f,"NATIVE_POOL name=%s calls=%u total_ms=%.6f per_selected_frame_ms=%.6f\n",
        names[i],calls[i],ticks[i]*1000.0/SYSCLOCK_ARM11,selectedFrames?ticks[i]*1000.0/SYSCLOCK_ARM11/selectedFrames:0);
}
}
#endif

#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
extern bool g_picaNativeTexturesEnabled;
#ifdef SRR2_OLD3DS_NATIVE_ETC1
extern bool g_picaNativeEtcEnabled;
#endif
#ifdef SRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE
extern bool g_picaNativeBundleEnabled;
#endif
extern bool g_picaNativeTextureDiagnostics;
#include <pddi/pica/texture.hpp>
static picaNativeTextureStats nativeTextureCaptureBegin={},nativeTextureCaptureEnd={};
extern "C" void picaDumpNativeTextures(FILE*);
#endif
extern "C" unsigned g_picaPresentedFrames;
extern bool g_old3dsTrafficFacingOnly;
extern bool g_old3dsAdpcmPrune,g_old3dsAdpcmVerify,g_old3dsAdpcmFast;
extern void old3dsDumpAdpcmVerification(FILE*);
extern unsigned g_old3dsAdpcmFastCalls,g_old3dsAdpcmFastFallbacks,g_old3dsAdpcmBudgetCalls;
extern unsigned g_old3dsAdpcmVerified,g_old3dsAdpcmMismatches,g_old3dsAdpcmVerifyAllocFailures;
namespace NativeBench {
bool detail=false,collecting=false,validation=false,serviceProfile=false;
static bool vehicleProfile=false;
static unsigned loaderSliceUs=0;
#ifdef SRR2_OLD3DS_SUBMIT_PROFILE
bool submissionProfile=false;
#endif
static void* captureTls=nullptr;
bool OnCaptureThread(){return getThreadLocalStorage()==captureTls;}
static unsigned behaviorHash=2166136261u;
void Hash(unsigned v){if(collecting && validation)behaviorHash=(behaviorHash^v)*16777619u;}
void HashFloat(float f){unsigned v;memcpy(&v,&f,4);Hash(v);}
unsigned counters[CountC];
static const unsigned Capacity=2048, Bins=2048;
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
static unsigned bottomMapMode=1,bottomMapStart=0;
static unsigned bottomMapSeed=0;
#endif
struct Record {
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 unsigned bottomMapEnabled,bottomMapDraws;
#endif
 unsigned nativeContext, frame, elapsed, context, period, realElapsed, stage[Count],exclusive[Count],count[CountC];
 unsigned work,draws,vertices,linear,heapUsed,loading,substeps,check,behavior,presentBegin,presentEnd;
 unsigned route, pairs, areas, entities; float position[3],speed,gpu; unsigned incar; };
static Record* records=nullptr;
static Record current;
static unsigned hist[Bins],frame,recordCount,overwritten,elapsedMs,routeMs,seconds=25,warmup=8,seed=231171,step=67;
static unsigned samples, misses, maximum,drawStart,vertStart,subStart,pairStart,areaStart,entityStart;
static unsigned entityCalls(){unsigned n=0;for(unsigned i=0;i<O3_CATEGORY_COUNT;++i)n+=g_old3dsEntities[i].calls;return n;}
static bool previousCaptured=false;
static u64 lastBeginTick,periodSum;
static unsigned periodSamples;
static u64 totalWork,traceTicks,scopeCalibration;
static unsigned verifyAll,verifyAllFailures;
static bool hold=false,reload=false,visualCapture=false;
static float stereoTestSlider=-1.0f;
bool StereoVisualAudit(){return visualCapture;}
float StereoSlider(float physical){return stereoTestSlider>=0.0f?stereoTestSlider:physical;}
// Development-only character inspection. Zero preserves the driving route.
static unsigned portrait=0;
static unsigned portraitLevel=2;
static bool checkpointIO=false;
static unsigned diagnostic=0;
static bool residentShaders=false;
#ifdef SRR2_OLD3DS_NATIVE_UNIFORMS
static bool nativeUniforms=true;
#else
static bool nativeUniforms=false;
#endif
static unsigned reloadPhase=0,reloadMs=0,reloadDispatches=0,reloadObservedLevels=0;
static bool reloadPauseRequested=false;
static bool enabled=true,replay=false,placed=false,done=false,aborted=false;
static u64 benchmarkStart,initializationTick=0,placementTick=0;
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
static unsigned nativeContextMode=1;
#else
static unsigned nativeContextMode=0;
#endif
static unsigned stableMs, warmMs, replayDrains, drainTimeouts, pendingAtBegin;
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
static unsigned contextFastBegin=0,contextFallbackBegin=0,contextWordsBegin=0;
static unsigned contextFastEnd=0,contextFallbackEnd=0,contextWordsEnd=0;
#endif
static Scope* top;
static const char* names[Count]={"update","render","stream","service","file_service","debug_service","sound_service","file_lock","file_callback","file_error","file_user_callback","sound_buffer_complete","sound_encode","sound_encode_kernel","load_handoff","load_callback","load_yield","load_lock"};
// Sparse development-only SD breadcrumbs survive a forced console restart.
// Formatting and I/O perturb the route; enabled captures are never FPS evidence.
static void Checkpoint(const char* boundary,int stage)
{
 if(!checkpointIO || !collecting || !frame || (frame&15))return;
 printf("NATIVE_CHECKPOINT frame=%u route_ms=%u boundary=%s stage=%s loading=%u presented=%u linear=%u performance_capture=0\n",
     frame,elapsedMs,boundary,stage>=0 && stage<Count?names[stage]:"frame",
     GetLoadingManager()->GetNumCurrentRequests(),g_picaPresentedFrames,unsigned(linearSpaceFree()));
}

void Add(Counter c,unsigned n){if(collecting)counters[c]+=n;}
void Scope::BeginScope(Stage s){Checkpoint("enter",s);stage=s;child=0;++counters[ScopeCalls];parent=top;top=this;start=svcGetSystemTick();}
void Scope::EndScope(){if(!start)return;u64 end=svcGetSystemTick(),dt=end-start;
 current.stage[stage]+=(unsigned)dt;current.exclusive[stage]+=(unsigned)(dt-child);
 if(parent)parent->child+=dt;top=parent;Checkpoint("leave",stage);}
unsigned LoaderSliceUs(){return loaderSliceUs;}
unsigned CurrentRouteMs(){return elapsedMs;}
bool LandmarkDiagnostic(){return false;}
bool ResidentShaders(){return residentShaders;}
bool NativeUniforms(){return nativeUniforms;}
// Benchmark-only input driver. Follow immutable road points through the normal
// controller; never move world objects or change simulation/traffic state.
static bool roadRoute=false;
static rmt::Vector roadPoints[256],roadLastPosition;
static unsigned roadPointCount=0,roadPoint=0,roadStalledMs=0,roadReverseMs=0,roadRecoveries=0;
static float roadDistance=0;
static int roadSteer=0,roadGas=0,roadBrake=0;
static bool BuildRoadRoute(Vehicle* vehicle)
{
 RoadSegment* segment=nullptr;float distance=0;
 GetIntersectManager()->FindClosestRoad(vehicle->GetPosition(),100,segment,distance);
 if(!segment)return false;
 const Road* road=segment->GetRoad();int index=segment->GetSegmentIndex();
 rmt::Vector a,b;segment->GetPosition(0,.5f,&a);segment->GetPosition(1,.5f,&b);
 const rmt::Vector& facing=vehicle->GetFacing();
 bool forward=(b.x-a.x)*facing.x+(b.z-a.z)*facing.z>=0;
 roadPoints[roadPointCount++]=forward?a:b;roadPoint=1;
 roadLastPosition=vehicle->GetPosition();
 for(unsigned roads=0;roads<32 && roadPointCount<256;++roads){
  while(index>=0 && index<(int)road->GetNumRoadSegments() && roadPointCount<256){
   segment=road->GetRoadSegment(index);
   segment->GetPosition(forward?1.f:0.f,.5f,&b);
   if(!roadPointCount || (b-roadPoints[roadPointCount-1]).MagnitudeSqr()>4)
    roadPoints[roadPointCount++]=b;
   index+=forward?1:-1;
  }
  if(roadPointCount<2)return false;
  rmt::Vector heading=roadPoints[roadPointCount-1]-roadPoints[roadPointCount-2];
  const float length=std::sqrt(heading.x*heading.x+heading.z*heading.z);
  if(length<.01f)break;
  heading.x/=length;heading.z/=length;
  const Intersection* intersection=forward?road->GetDestinationIntersection():road->GetSourceIntersection();
  if(!intersection)break;
  const Road* next=nullptr;bool nextForward=true;float best=-2;
  for(unsigned group=0;group<2;++group){
   const unsigned count=group?intersection->GetNumRoadsIn():intersection->GetNumRoadsOut();
   for(unsigned i=0;i<count;++i){
    const Road* candidate=group?intersection->GetRoadIn(i):intersection->GetRoadOut(i);
    if(candidate==road || candidate->GetShortCut() || !candidate->GetNumRoadSegments())continue;
    RoadSegment* entry=candidate->GetRoadSegment(group?candidate->GetNumRoadSegments()-1:0);
    entry->GetPosition(group?1.f:0.f,.5f,&a);entry->GetPosition(group?0.f:1.f,.5f,&b);
    const float dx=b.x-a.x,dz=b.z-a.z,n=std::sqrt(dx*dx+dz*dz);
    if(n<.01f)continue;
    const float alignment=(dx*heading.x+dz*heading.z)/n;
    if(alignment>best){best=alignment;next=candidate;nextForward=!group;}
   }
  }
  if(!next)break;
  road=next;forward=nextForward;index=forward?0:road->GetNumRoadSegments()-1;
 }
 return roadPointCount>=8;
}
static void UpdateRoadInput(Vehicle* vehicle,unsigned elapsed)
{
 const rmt::Vector position=vehicle->GetPosition(),facing=vehicle->GetFacing();
 roadDistance+=(position-roadLastPosition).Magnitude();roadLastPosition=position;
 while(roadPoint+1<roadPointCount){
  const rmt::Vector delta=roadPoints[roadPoint]-position;
  if(delta.x*delta.x+delta.z*delta.z>100)break;
  ++roadPoint;
 }
 const rmt::Vector target=roadPoints[roadPoint]-position;
 const float angle=std::atan2(facing.z*target.x-facing.x*target.z,facing.x*target.x+facing.z*target.z);
 float steering=angle/.6f;if(steering>1)steering=1;if(steering<-1)steering=-1;
 roadSteer=int(steering*26000);
 const float speed=vehicle->mSpeed;
 const float desired=std::fabs(angle)>.6f?9.f:18.f;
 roadGas=speed<desired;roadBrake=speed>desired+3.f;
 if(roadReverseMs){
  roadReverseMs=elapsed>=roadReverseMs?0:roadReverseMs-elapsed;
  roadGas=0;roadBrake=1;roadSteer=-roadSteer;
 }else{
  roadStalledMs=speed<.75f && roadGas?roadStalledMs+elapsed:0;
  if(roadStalledMs>1800){roadReverseMs=1200;roadStalledMs=0;++roadRecoveries;}
 }
}

static unsigned saveFixture=0;
static unsigned fixtureBytes[4]={},fixtureHash[4]={};
void SaveFilename(char* name,unsigned bytes,unsigned slot)
{snprintf(name,bytes,"B143%08xSave%u",saveFixture,slot+1);}
static bool PrepareSaveFixtures()
{
 saveFixture=unsigned(osGetTime());
 // Every launch gets private copies. The engine cannot autosave over the
 // owner's Save1..Save4 or reuse a previous benchmark's modified save.
 for(unsigned attempt=0;attempt<16;++attempt,++saveFixture){
  bool collision=false;struct stat st;char target[32];
  for(unsigned i=0;i<4;++i){SaveFilename(target,sizeof(target),i);if(!stat(target,&st))collision=true;}
  if(collision)continue;
  for(unsigned i=0;i<4;++i){
   char source[16];snprintf(source,sizeof(source),"Save%u",i+1);FILE* input=fopen(source,"rb");
   if(!input){if(errno==ENOENT)continue;return false;}
   SaveFilename(target,sizeof(target),i);int descriptor=open(target,O_CREAT|O_EXCL|O_WRONLY,0666);
   if(descriptor<0){fclose(input);return false;}FILE* output=fdopen(descriptor,"wb");
   if(!output){close(descriptor);fclose(input);return false;}
   unsigned char buffer[4096];size_t count;bool ok=true;unsigned hash=2166136261u,total=0;
   while((count=fread(buffer,1,sizeof(buffer),input))){
    total+=count;if(total>1024*1024 || fwrite(buffer,1,count,output)!=count){ok=false;break;}
    for(size_t j=0;j<count;++j)hash=(hash^buffer[j])*16777619u;
   }
   if(ferror(input))ok=false;fclose(input);if(fclose(output))ok=false;
   fixtureBytes[i]=total;fixtureHash[i]=hash;
   if(!ok)return false;
  }
  return true;
 }
 return false;
}
static char runIdentity[64]="unspecified";
static bool ParseRunArgument(const char* argument)
{
 static const char duration[]="bench_seconds=",warm[]="bench_warmup=",keep[]="bench_hold=",
     restart[]="bench_reload=",random[]="bench_seed=",identity[]="bench_trial=";
 if(!strncmp(argument,duration,sizeof(duration)-1))seconds=strtoul(argument+sizeof(duration)-1,nullptr,10);
 else if(!strncmp(argument,warm,sizeof(warm)-1))warmup=strtoul(argument+sizeof(warm)-1,nullptr,10);
 else if(!strncmp(argument,keep,sizeof(keep)-1))hold=atoi(argument+sizeof(keep)-1)!=0;
 else if(!strncmp(argument,restart,sizeof(restart)-1))reload=atoi(argument+sizeof(restart)-1)!=0;
 else if(!strncmp(argument,random,sizeof(random)-1))seed=strtoul(argument+sizeof(random)-1,nullptr,10);
 else if(!strncmp(argument,"bench_quality=",14))Old3dsPerformance::mode=strtoul(argument+14,nullptr,10)%8;
 else if(!strncmp(argument,"bench_adpcm_fast=",17))g_old3dsAdpcmFast=atoi(argument+17)!=0;
 else if(!strncmp(argument,"bench_adpcm_verify=",19))g_old3dsAdpcmVerify=atoi(argument+19)!=0;
 else if(!strncmp(argument,"bench_adpcm_prune=",18))g_old3dsAdpcmPrune=atoi(argument+18)!=0;
 else if(!strncmp(argument,"bench_traffic_facing=",21))g_old3dsTrafficFacingOnly=atoi(argument+21)!=0;
 else if(!strncmp(argument,"bench_vehicle_profile=",22))vehicleProfile=atoi(argument+22)!=0;
 else if(!strncmp(argument,"bench_recurring=",16))Old3dsRecurring::requested=atoi(argument+16)!=0;
 else if(!strncmp(argument,"bench_stationary_ms=",20))Old3dsRecurring::stationaryMs=strtoul(argument+20,nullptr,10);
 else if(!strncmp(argument,"bench_service_profile=",22))serviceProfile=atoi(argument+22)!=0;
 else if(!strncmp(argument,"bench_stereo_slider=",20)){
  char* end=nullptr;const float value=strtof(argument+20,&end);
  if(end!=argument+20 && !*end && std::isfinite(value) && value>=0 && value<=1)stereoTestSlider=value;
 }
 else if(!strncmp(argument,identity,sizeof(identity)-1))snprintf(runIdentity,sizeof(runIdentity),"%s",argument+sizeof(identity)-1);
 else return false;
 return true;
}
void Init(int argc,char** argv){
 captureTls=getThreadLocalStorage();
 initializationTick=svcGetSystemTick();
#if defined(SRR2_OLD3DS_VEHICLE_PROFILE) || defined(SRR2_OLD3DS_NATIVE_BENCH)
 Old3dsVehicleProfile::CaptureEnabled()=false;
#endif
 if(!PrepareSaveFixtures()){printf("NATIVE_AUTOMATION save_fixture_failed abort=1\n");aborted=done=true;return;}
 FILE* f=fopen("native-automation.txt","r");
 // This development-only build defaults to the real-time route without SD setup.
 if(f){
 char key[64],value[64];while(fscanf(f,"%63[^=]=%63s\n",key,value)==2){
 if(!strcmp(key,"recurring"))Old3dsRecurring::requested=atoi(value)!=0;
 else if(!strcmp(key,"quality"))Old3dsPerformance::mode=strtoul(value,nullptr,10)%8;
 else if(!strcmp(key,"stationary_ms"))Old3dsRecurring::stationaryMs=strtoul(value,nullptr,10);
 else if(!strcmp(key,"mode"))replay=!strcmp(value,"replay");
 else if(!strcmp(key,"seconds"))seconds=strtoul(value,0,10);
 else if(!strcmp(key,"warmup"))warmup=strtoul(value,0,10);
 else if(!strcmp(key,"seed"))seed=strtoul(value,0,10);
 else if(!strcmp(key,"step_ms"))step=strtoul(value,0,10);
 else if(!strcmp(key,"detail"))detail=atoi(value)!=0;
 // Historical native experiment validation is deliberately not restored.
 else if(!strcmp(key,"diagnostic"))diagnostic=strtoul(value,0,10);
 else if(!strcmp(key,"route"))roadRoute=!strcmp(value,"road");
 #ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 else if(!strcmp(key,"bottom_seed"))bottomMapSeed=strtoul(value,nullptr,10);
 else if(!strcmp(key,"bottom_map"))bottomMapMode=strtoul(value,nullptr,10)%3;
 #endif
#ifdef SRR2_OLD3DS_MEMORY_DIAG
 else if(!strcmp(key,"memory_stride"))Old3dsMemorySetStride(strtoul(value,nullptr,10));
#endif
#ifdef SRR2_OLD3DS_MEMORY_DIAG
 else if(!strcmp(key,"memory_stride"))Old3dsMemorySetStride(strtoul(value,nullptr,10));
#endif
 else if(!strcmp(key,"reload"))reload=atoi(value)!=0;
 else if(!strcmp(key,"hold"))hold=atoi(value)!=0;
#ifdef SRR2_OLD3DS_GEOMETRY_INPUT
 else if(!strcmp(key,"geometry_input"))p3dGeometryInput::enabled=atoi(value)!=0;
#endif
 else if(!strcmp(key,"visual_capture"))visualCapture=atoi(value)!=0;
 else if(!strcmp(key,"portrait"))portrait=strtoul(value,nullptr,10)%4;
 else if(!strcmp(key,"portrait_level"))portraitLevel=strtoul(value,nullptr,10);
#ifdef SRR2_OLD3DS_DISTANCE_HAZE
 else if(!strcmp(key,"distance_haze"))picaDistanceHaze::enabled=atoi(value)!=0;
#endif
#ifdef SRR2_OLD3DS_CHARACTER_FORM
 else if(!strcmp(key,"character_form"))picaCharacterForm::enabled=atoi(value)!=0;
#endif
 else if(!strcmp(key,"checkpoint_io"))checkpointIO=atoi(value)!=0;
#ifndef SRR2_OLD3DS_CAUSAL_DIAG
 else if(!strcmp(key,"cadence"))Old3dsCadence::enabled=atoi(value)!=0;
#endif
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
 else if(!strcmp(key,"context"))nativeContextMode=strtoul(value,0,10)%3;
#endif
 else if(!strcmp(key,"uniforms"))nativeUniforms=atoi(value)!=0;
#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
 #ifdef SRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE
 else if(!strcmp(key,"native_bundle"))g_picaNativeBundleEnabled=atoi(value)!=0;
 #endif
#ifdef SRR2_OLD3DS_NATIVE_ETC1
 else if(!strcmp(key,"native_etc1"))g_picaNativeEtcEnabled=atoi(value)!=0;
#endif
 else if(!strcmp(key,"native_textures"))g_picaNativeTexturesEnabled=atoi(value)!=0;
 else if(!strcmp(key,"texture_diagnostics"))g_picaNativeTextureDiagnostics=atoi(value)!=0;
#endif
 }fclose(f);}
 // Netloader arguments override the optional file without modifying the SD.
 // One exact executable can run matched reference/candidate investigations.

#ifdef SRR2_OLD3DS_COARSE_POOLS
 Old3dsPools::Configure(0);
#endif
 for(int i=1;i<argc;++i){
  if(ParseRunArgument(argv[i]))continue;
#ifdef SRR2_OLD3DS_GEOMETRY_INPUT
  static const char geometryArgument[]="bench_geometry_input=";
  if(!strncmp(argv[i],geometryArgument,sizeof(geometryArgument)-1)){p3dGeometryInput::enabled=atoi(argv[i]+sizeof(geometryArgument)-1)!=0;continue;}
#endif
  static const char checkpointArgument[]="bench_checkpoint_io=";
  if(!strncmp(argv[i],checkpointArgument,sizeof(checkpointArgument)-1)){checkpointIO=atoi(argv[i]+sizeof(checkpointArgument)-1)!=0;continue;}
  static const char visualArgument[]="bench_visual_capture=";
  if(!strncmp(argv[i],visualArgument,sizeof(visualArgument)-1)){visualCapture=atoi(argv[i]+sizeof(visualArgument)-1)!=0;continue;}
  static const char portraitArgument[]="bench_portrait=";
  if(!strncmp(argv[i],portraitArgument,sizeof(portraitArgument)-1)){portrait=strtoul(argv[i]+sizeof(portraitArgument)-1,nullptr,10)%4;continue;}
  static const char portraitLevelArgument[]="bench_portrait_level=";
  if(!strncmp(argv[i],portraitLevelArgument,sizeof(portraitLevelArgument)-1)){portraitLevel=strtoul(argv[i]+sizeof(portraitLevelArgument)-1,nullptr,10);continue;}
#ifdef SRR2_OLD3DS_DISTANCE_HAZE
  static const char hazeArgument[]="bench_distance_haze=";
  if(!strncmp(argv[i],hazeArgument,sizeof(hazeArgument)-1)){picaDistanceHaze::enabled=atoi(argv[i]+sizeof(hazeArgument)-1)!=0;continue;}
#endif
#ifdef SRR2_OLD3DS_CHARACTER_FORM
  static const char formArgument[]="bench_character_form=";
  if(!strncmp(argv[i],formArgument,sizeof(formArgument)-1)){picaCharacterForm::enabled=atoi(argv[i]+sizeof(formArgument)-1)!=0;continue;}
#endif
#ifdef SRR2_OLD3DS_MEMORY_DIAG
  static const char memoryStrideOption[]= "bench_memory_stride=";
  if(!strncmp(argv[i],memoryStrideOption,sizeof(memoryStrideOption)-1)){Old3dsMemorySetStride(strtoul(argv[i]+sizeof(memoryStrideOption)-1,nullptr,10));continue;}
#endif
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
  static const char bottomSeedArgument[]="bench_bottom_seed=";
  if(!strncmp(argv[i],bottomSeedArgument,sizeof(bottomSeedArgument)-1)){bottomMapSeed=strtoul(argv[i]+sizeof(bottomSeedArgument)-1,nullptr,10);continue;}
  if(!strncmp(argv[i],"bench_bottom_map=",17)){bottomMapMode=strtoul(argv[i]+17,nullptr,10)%3;continue;}
#endif
  if(!strncmp(argv[i],"bench_route=",12)){roadRoute=!strcmp(argv[i]+12,"road");continue;}
#ifdef SRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE
  if(!strncmp(argv[i],"bench_native_bundle=",20)){g_picaNativeBundleEnabled=atoi(argv[i]+20)!=0;continue;}
#endif
#ifndef SRR2_OLD3DS_CAUSAL_DIAG
  if(!strncmp(argv[i],"bench_cadence=",14)){Old3dsCadence::enabled=atoi(argv[i]+14)!=0;continue;}
#endif
#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
#ifdef SRR2_OLD3DS_NATIVE_ETC1
  static const char etcArgument[]="bench_native_etc1=";
  if(!strncmp(argv[i],etcArgument,sizeof(etcArgument)-1)){g_picaNativeEtcEnabled=atoi(argv[i]+sizeof(etcArgument)-1)!=0;continue;}
#endif
  static const char nativeTextureArgument[]="bench_native_textures=";
  static const char textureDiagnosticArgument[]="bench_texture_diagnostics=";
  if(!strncmp(argv[i],textureDiagnosticArgument,sizeof(textureDiagnosticArgument)-1)){g_picaNativeTextureDiagnostics=atoi(argv[i]+sizeof(textureDiagnosticArgument)-1)!=0;continue;}
  if(!strncmp(argv[i],nativeTextureArgument,sizeof(nativeTextureArgument)-1)){g_picaNativeTexturesEnabled=atoi(argv[i]+sizeof(nativeTextureArgument)-1)!=0;continue;}
#endif
#ifdef SRR2_OLD3DS_COARSE_POOLS
  static const char poolArgument[]="bench_pool=";
  if(!strncmp(argv[i],poolArgument,sizeof(poolArgument)-1)){Old3dsPools::Configure(strtoul(argv[i]+sizeof(poolArgument)-1,nullptr,10));continue;}
#endif
#ifdef SRR2_OLD3DS_MAIN_PROFILE
  static const char mainProfileArgument[]="bench_main_profile=";
  if(!strncmp(argv[i],mainProfileArgument,sizeof(mainProfileArgument)-1)){Old3dsMainProfile::Configure(strtoul(argv[i]+sizeof(mainProfileArgument)-1,nullptr,10)!=0);continue;}
#endif
  if(!strncmp(argv[i],"bench_mode=",11))replay=!strcmp(argv[i]+11,"replay");
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
  else if(!strncmp(argv[i],"bench_context=",14))nativeContextMode=strtoul(argv[i]+14,nullptr,10)%3;
#endif
  else if(!strncmp(argv[i],"bench_resident=",15))residentShaders=atoi(argv[i]+15)!=0;
  else if(!strncmp(argv[i],"bench_uniforms=",15))nativeUniforms=atoi(argv[i]+15)!=0;
  else if(!strncmp(argv[i],"bench_detail=",13))detail=atoi(argv[i]+13)!=0;
  else if(!strncmp(argv[i],"bench_diagnostic=",17))diagnostic=strtoul(argv[i]+17,nullptr,10);
  else if(!strncmp(argv[i],"bench_step=",11))step=strtoul(argv[i]+11,nullptr,10);
 }
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
 g_picaNativeContextEnabled=nativeContextMode==1;
#endif
 if(diagnostic>4)diagnostic=0;
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
 O3ExperimentExternal(diagnostic,false);
#endif
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 Old3dsBottomMap::enabled=bottomMapMode!=0;
#endif
 if(seconds<5||seconds>(roadRoute?60u:25u))seconds=25;if(warmup<3||warmup>60)warmup=8;if(step<1||step>100)step=67;
 // Use the existing normal heap after libctru startup, not additional BSS.
 records=static_cast<Record*>(calloc(Capacity,sizeof(Record)));
 printf("NATIVE_LAUNCH id=%s mode=%s seconds=%u checkpoint_io=%u\n",runIdentity,replay?"replay":"realtime",seconds,checkpointIO);
 if(!records){enabled=false;aborted=true;done=true;
  printf("NATIVE_AUTOMATION allocation_failed bytes=%u abort=1\n",unsigned(Capacity*sizeof(Record)));return;}
 bool savedDetail=detail;detail=true;collecting=true;u64 cal=svcGetSystemTick();for(unsigned i=0;i<4096;++i){Scope test(Update);}scopeCalibration=svcGetSystemTick()-cal;collecting=false;detail=savedDetail;
 char a[]="skipfe",b[]="notutorial",c[]="L2";
 if(portrait && portraitLevel>=1 && portraitLevel<=7)c[1]='0'+portraitLevel;
 CommandLineOptions::HandleOption(a);CommandLineOptions::HandleOption(b);CommandLineOptions::HandleOption(c);
}
unsigned Seed(unsigned fallback){return enabled?seed:fallback;}
unsigned Begin(unsigned elapsed,int context){
 u64 now=svcGetSystemTick();
 if(previousCaptured){unsigned ticks=(unsigned)(now-lastBeginTick);records[(recordCount-1)%Capacity].period=ticks;periodSum+=ticks;++periodSamples;previousCaptured=false;}
 lastBeginTick=now;
 collecting=false;
#if defined(SRR2_OLD3DS_VEHICLE_PROFILE) || defined(SRR2_OLD3DS_NATIVE_BENCH)
 Old3dsVehicleProfile::CaptureEnabled()=false;
#endif
 if(!enabled)return elapsed;
#ifdef SRR2_OLD3DS_MEMORY_DIAG
 const bool routeReady=placed && warmMs>=warmup*1000;
 const bool loading=GetLoadingManager()->IsLoading();
 Old3dsMemorySetPhase(done && reload?O3MemoryReload:context!=CONTEXT_GAMEPLAY?O3MemoryLoading:
     loading?(routeReady?O3MemoryStreaming:O3MemoryLoading):routeReady?O3MemoryRoute:O3MemoryWarmup);
#endif
 if(done){
  if(reload && reloadPhase<3){
   CGuiManagerInGame* manager=GetGuiSystem()->GetInGameManager();
   if(context==CONTEXT_GAMEPLAY && !GetLoadingManager()->IsLoading()){
    const int expected=reloadPhase==1?RenderEnums::L1:RenderEnums::L2;
    if((int)GetGameplayManager()->GetCurrentLevelIndex()==expected){
     reloadObservedLevels|=1u<<expected;
     reloadMs+=elapsed;
     if(reloadPhase==0 || reloadMs>=10000){
      if(reloadPhase==2)reloadPhase=3;
      else if(!reloadPauseRequested){
       // Mission Select normally enters PauseContext before requesting reload.
       // Its OnStop owns teardown and publication of the next level/mission.
       manager->HandleMessage(GUI_MSG_PAUSE_INGAME);reloadPauseRequested=true;
      }
     }
    }
   }
   if(reloadPauseRequested && context==CONTEXT_PAUSE && manager->GetCurrentWindow()
       && manager->GetCurrentWindow()->IsRunning()){
    manager->HandleMessage(GUI_MSG_QUIT_INGAME_FOR_RELOAD,
        reloadPhase==0?RenderEnums::L1:RenderEnums::L2,RenderEnums::M2);
    ++reloadPhase;++reloadDispatches;reloadMs=0;reloadPauseRequested=false;
   }
  }
  if(reload && now-benchmarkStart>300ULL*SYSCLOCK_ARM11){aborted=true;reloadPhase=3;}
  return elapsed;
 }
 if(!benchmarkStart)benchmarkStart=now;
 if(now-benchmarkStart>300ULL*SYSCLOCK_ARM11){aborted=true;done=true;return elapsed;}
 unsigned dt=replay?step:elapsed;
 pendingAtBegin=GetLoadingManager()->GetNumCurrentRequests();
 if(replay && context==CONTEXT_GAMEPLAY && GetLoadingManager()->IsLoading()){
  // Replay alone makes resource completion a simulation barrier. Real-time runs
  // retain the original asynchronous loading schedule and elapsed-time policy.
  ++replayDrains;u64 start=svcGetSystemTick();
  while(GetLoadingManager()->IsLoading()){
   radFileService();p3d::loadManager->SwitchTask();p3d::loadManager->TriggerCallbacks();
   svcSleepThread(1000000);
   if(svcGetSystemTick()-start>5ULL*SYSCLOCK_ARM11){++drainTimeouts;break;}
  }
 }
 if(context!=CONTEXT_GAMEPLAY){stableMs=0;return dt;}
 Avatar* avatar=GetAvatarManager()->GetAvatarForPlayer(0);
 if(!placed){
 stableMs+=dt;
 if(GetLoadingManager()->IsLoading()||!avatar||!avatar->GetCharacter()||!GetGameplayManager()->GetCurrentVehicle()){if(!replay)stableMs=0;return dt;}
 if(!replay && stableMs<3000u)return dt;
 // Use the level's original car reset and mount facilities to normalize setup.
 // No objects are removed and no reset occurs inside the measured route.
 GetGameplayManager()->GetCurrentVehicle()->Reset(true,false);
 GetAvatarManager()->PutCharacterInCar(avatar->GetCharacter(),GetGameplayManager()->GetCurrentVehicle());
 avatar->SetCameraTargetToVehicle(true);
 GetSuperCamManager()->GetSCC(0)->SelectSuperCam(SuperCam::FAR_FOLLOW_CAM,SuperCamCentral::CUT|SuperCamCentral::FORCE);
 if(portrait){
  GetAvatarManager()->PutCharacterOnGround(avatar->GetCharacter(),GetGameplayManager()->GetCurrentVehicle());
  avatar->SetCameraTargetToCharacter(true);
  Character* character=avatar->GetCharacter();
  rmt::Vector position;character->GetPosition(position);
  character->RelocateAndReset(position,0.0f);
  SuperCamCentral* central=GetSuperCamManager()->GetSCC(0);
  StaticCam* camera=static_cast<StaticCam*>(central->GetSuperCam(SuperCam::STATIC_CAM));
  if(!camera){camera=new StaticCam();central->RegisterSuperCam(camera);}
  camera->SetTarget(character->GetTarget());
  camera->SetTargetOffset(rmt::Vector(0.0f,0.85f,0.0f));
  camera->SetTargetLag(1.0f);
  camera->SetTracking(true);
  const float side=portrait==3?2.8f:0.0f;
  const float front=portrait==2?-3.3f:3.3f;
  camera->SetPosition(rmt::Vector(position.x+side,position.y+1.2f,position.z+front));
  central->SelectSuperCam(camera,SuperCamCentral::CUT|SuperCamCentral::FORCE);
 }
 placed=true;placementTick=svcGetSystemTick();
 if(roadRoute && !portrait && !BuildRoadRoute(GetGameplayManager()->GetCurrentVehicle())){aborted=done=true;return dt;}
 }
 if(warmMs<warmup*1000){warmMs+=dt;return dt;}
#if defined(SRR2_OLD3DS_VEHICLE_PROFILE) || defined(SRR2_OLD3DS_NATIVE_BENCH)
 // Only measured route work contributes to the phase estimates. Loading and
 // warmup have different vehicle populations and must not bias the cost pool.
 if(!frame){Old3dsVehicleProfile::counters={};Old3dsVehicleProfile::externalCounters={};
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
  picaNativeContextStats(&contextFastBegin,&contextFallbackBegin,&contextWordsBegin);
#endif
 }
 Old3dsVehicleProfile::CaptureEnabled()=vehicleProfile;
#endif
#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
 if(!frame)picaSnapshotNativeTextures(nativeTextureCaptureBegin);
#endif
 if(roadRoute && !portrait){if(elapsedMs<Old3dsRecurring::stationaryMs){roadGas=0;roadBrake=0;roadSteer=0;}else UpdateRoadInput(GetGameplayManager()->GetCurrentVehicle(),dt);}
 memset(&current,0,sizeof(current));memset(counters,0,sizeof(counters));
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 unsigned bottomChoice=((frame/24)^bottomMapSeed)*0x9e3779b9u;bottomChoice^=bottomChoice>>16;bottomChoice*=0x85ebca6bu;bottomChoice^=bottomChoice>>13;
 Old3dsBottomMap::enabled=bottomMapMode==1 || (bottomMapMode==2 && (bottomChoice&1));
 current.bottomMapEnabled=Old3dsBottomMap::enabled;bottomMapStart=Old3dsBottomMap::DrawCount();
#endif

#ifdef SRR2_OLD3DS_COARSE_POOLS
 Old3dsPools::Begin(frame+1);
#endif
 current.route=routeMs;current.frame=++frame;current.elapsed=dt;current.realElapsed=elapsed;current.context=context;
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
 // Deterministic per-frame randomization avoids separate-run traffic confounding.
 unsigned choice=frame*0x9e3779b9u;choice^=choice>>16;choice*=0x85ebca6bu;choice^=choice>>13;
 const bool contextEnabled=nativeContextMode==1 || (nativeContextMode==2 && (choice&1));
 if(g_picaNativeContextEnabled!=contextEnabled){
  g_picaNativeContextEnabled=contextEnabled;
 }
 current.nativeContext=g_picaNativeContextEnabled;
#endif
 routeMs=elapsedMs;elapsedMs+=dt;collecting=true;top=0;behaviorHash=2166136261u;
 Checkpoint("enter",-1);
 pairStart=g_old3dsCollisionPairs;areaStart=g_old3dsPhysicsAreas;entityStart=entityCalls();
 current.presentBegin=g_picaPresentedFrames;
 drawStart=g_old3dsDrawCalls;vertStart=g_old3dsDrawVerts;subStart=g_old3dsPhysicsSubsteps;
#ifdef SRR2_OLD3DS_MAIN_PROFILE
 Old3dsMainProfile::BeginFrame(frame,collecting);
#endif
 Old3dsRecurring::Begin(frame,routeMs,collecting);
 return dt;
}
static unsigned hashFloat(unsigned h,float f){unsigned v;memcpy(&v,&f,4);return (h^v)*16777619u;}
bool End(u64 work,int context){
 Old3dsRecurring::End();
#ifdef SRR2_OLD3DS_COARSE_POOLS
 Old3dsPools::End();
#endif
#ifdef SRR2_OLD3DS_MAIN_PROFILE
 Old3dsMainProfile::EndFrame(work);
#endif
 if(!collecting)return aborted || (done && reload && reloadPhase==3 && !hold);Checkpoint("leave",-1);u64 overhead=svcGetSystemTick();collecting=false;
#if defined(SRR2_OLD3DS_VEHICLE_PROFILE) || defined(SRR2_OLD3DS_NATIVE_BENCH)
 Old3dsVehicleProfile::CaptureEnabled()=false;
#endif
 current.pairs=g_old3dsCollisionPairs-pairStart;current.areas=g_old3dsPhysicsAreas-areaStart;current.entities=entityCalls()-entityStart;
 current.work=(unsigned)work;current.context=context;current.draws=g_old3dsDrawCalls-drawStart;current.vertices=g_old3dsDrawVerts-vertStart;
 // Heap walks are intrusive on the fragmented gameplay heap. Timing builds
 // leave this field unmeasured; dedicated allocation diagnostics retain peaks.
#ifdef SRR2_OLD3DS_MEMORY_DIAG
 current.heapUsed=mallinfo().uordblks;
#else
 current.heapUsed=0;
#endif
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 current.bottomMapDraws=Old3dsBottomMap::DrawCount()-bottomMapStart;
#endif
 current.presentEnd=g_picaPresentedFrames;
 current.linear=linearSpaceFree();current.loading=GetLoadingManager()->GetNumCurrentRequests()+pendingAtBegin;current.substeps=g_old3dsPhysicsSubsteps-subStart;
 memcpy(current.count,counters,sizeof(counters));Avatar* a=GetAvatarManager()->GetAvatarForPlayer(0);
 if(a){rmt::Vector p;a->GetPosition(p);current.position[0]=p.x;current.position[1]=p.y;current.position[2]=p.z;current.speed=a->GetSpeedMps();current.incar=a->IsInCar();
 rmt::Vector v;a->GetVelocity(v);unsigned h=2166136261u;h=hashFloat(h,p.x);h=hashFloat(h,p.y);h=hashFloat(h,p.z);h=hashFloat(h,v.x);h=hashFloat(h,v.y);h=hashFloat(h,v.z);current.check=h;}
 current.behavior=behaviorHash;current.gpu=C3D_GetDrawingTime();unsigned us=(unsigned)(work*1000000ULL/SYSCLOCK_ARM11);unsigned bin=us/250;if(bin>=Bins)bin=Bins-1;++hist[bin];
 ++samples;totalWork+=work;if(us>33333)++misses;if(us>maximum)maximum=us;
 records[recordCount++%Capacity]=current;previousCaptured=true;if(recordCount>Capacity)++overwritten;
 traceTicks+=svcGetSystemTick()-overhead;
 done=elapsedMs>=seconds*1000;
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
 if(done)picaNativeContextStats(&contextFastEnd,&contextFallbackEnd,&contextWordsEnd);
#endif
#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
 if(done)picaSnapshotNativeTextures(nativeTextureCaptureEnd);
#endif
 return done && !hold && !reload;
}
// Development-only native framebuffer readback after the route has stopped.
// The SDK exposes the renderable back buffer, so its age is deliberately not
// treated as a displayed frame identity. Never use this capture for FPS proof.
static void DumpVisualEye(FILE* f,gfx3dSide_t eye,const char* tag)
{
 u16 width=0,height=0;
 const GSPGPU_FramebufferFormat format=gfxGetScreenFormat(GFX_TOP);
 unsigned char* pixels=gfxGetFramebuffer(GFX_TOP,eye,&width,&height);
 const unsigned bytes=unsigned(width)*height*3;
 const bool valid=pixels && format==GSP_BGR8_OES && width==240 && (height==400 || height==800) &&
     R_SUCCEEDED(GSPGPU_InvalidateDataCache(pixels,bytes));
 fprintf(f,"%s enabled=1 valid=%u screen=top raw_width=%u raw_height=%u format=%u back_buffer=1 age_unknown=1 after_route=1 performance_capture=0\n",tag,valid,width,height,unsigned(format));
 if(!valid)return;
 static const char hex[]="0123456789abcdef";
 char line[513];
 for(unsigned offset=0;offset<bytes;offset+=256){
  const unsigned count=bytes-offset<256?bytes-offset:256;
  for(unsigned i=0;i<count;++i){const unsigned value=pixels[offset+i];line[i*2]=hex[value>>4];line[i*2+1]=hex[value&15];}
  line[count*2]=0;fprintf(f,"%s_BYTES offset=%u data=%s\n",tag,offset,line);
 }
 fprintf(f,"%s_END bytes=%u\n",tag,bytes);
}
static void DumpVisual(FILE* f)
{
 if(!visualCapture)return;
 C3D_FrameSync();
 DumpVisualEye(f,GFX_LEFT,"NATIVE_VISUAL");
 if(Graphics3DS::Stereo())DumpVisualEye(f,GFX_RIGHT,"NATIVE_VISUAL_RIGHT");
}
extern "C" void picaDumpStereoDiagnostics(FILE*);
void Dump(){if(!enabled)return;
 FILE*f=fopen("native-" SRR2_OLD3DS_BUILD_ID ".log","w");if(!f)return;
 unsigned optionalDiagnostics=0,memoryDiagnostics=0;
#ifdef SRR2_OLD3DS_OPTIONAL_DIAG
 optionalDiagnostics=1;
#endif
#ifdef SRR2_OLD3DS_MEMORY_DIAG
 memoryDiagnostics=1;
#endif
 Old3dsRecurring::Dump(f);
 picaDumpStereoDiagnostics(f);
 fprintf(f,"NATIVE_STEREO_TEST slider_override=%.4f command_audit=%u performance_capture=%u\n",stereoTestSlider,visualCapture,!visualCapture && stereoTestSlider<0.0f);
 fprintf(f,"NATIVE_FUSED_TRIG enabled=1 nearest_only=1 bounded_radians=8192 per_call_counters=0 runtime_selector=0\n");
 old3dsDumpAdpcmVerification(f);
 fprintf(f,"NATIVE_ADPCM_FAST enabled=%u calls=%u fallbacks=%u predictor_order_unchanged=%u budget_calls=%u budget_predictors=3 callbacks_deferred=0\n",g_old3dsAdpcmFast,g_old3dsAdpcmFastCalls,g_old3dsAdpcmFastFallbacks,g_old3dsAdpcmBudgetCalls==0,g_old3dsAdpcmBudgetCalls);
 fprintf(f,"NATIVE_ADPCM_VERIFY enabled=%u verified=%u mismatches=%u allocation_failures=%u performance_capture=%u\n",g_old3dsAdpcmVerify,g_old3dsAdpcmVerified,g_old3dsAdpcmMismatches,g_old3dsAdpcmVerifyAllocFailures,!g_old3dsAdpcmVerify);
 fprintf(f,"NATIVE_ADPCM_PRUNE enabled=%u exact_predictor_order=1 callbacks_deferred=0\n",g_old3dsAdpcmPrune);
 fprintf(f,"NATIVE_TRAFFIC_FACING enabled=%u\n",g_old3dsTrafficFacingOnly);
 fprintf(f,"NATIVE_SERVICE_PROFILE enabled=%u in_memory=1 post_capture_dump=1\n",serviceProfile);
 fprintf(f,"NATIVE_RUN id=%s warmup_seconds=%u\n",runIdentity,warmup);
 fprintf(f,"NATIVE_CHECKPOINT_MODE enabled=%u interval_frames=16 performance_capture=%u\n",checkpointIO,!checkpointIO);
 fprintf(f,"NATIVE_BUILD_FLAGS optional_diagnostics=%u memory_diagnostics=%u heap_frame_sampled=%u presentation_counts_are_submissions=1\n",optionalDiagnostics,memoryDiagnostics,memoryDiagnostics);
#ifdef SRR2_OLD3DS_GEOMETRY_INPUT
 fprintf(f,"NATIVE_GEOMETRY_INPUT enabled=%u records=%u reads=%u bytes=%u block_records=64 static_only=1 source_order_unchanged=1\n",p3dGeometryInput::enabled,__atomic_load_n(&p3dGeometryInput::records,__ATOMIC_RELAXED),__atomic_load_n(&p3dGeometryInput::reads,__ATOMIC_RELAXED),__atomic_load_n(&p3dGeometryInput::bytes,__ATOMIC_RELAXED));
#endif
 for(unsigned slot=0;slot<4;++slot)fprintf(f,"NATIVE_SAVE_FIXTURE id=%08x slot=%u bytes=%u fnv1a=%08x personal_save_writes=0\n",saveFixture,slot+1,fixtureBytes[slot],fixtureHash[slot]);

#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
 picaDumpNativeTextures(f);
 fprintf(f,"NATIVE_TEXTURES_CAPTURE hits=%u misses=%u invalid=%u bytes=%u completion_counts=1\n",
     nativeTextureCaptureEnd.hits-nativeTextureCaptureBegin.hits,nativeTextureCaptureEnd.misses-nativeTextureCaptureBegin.misses,
     nativeTextureCaptureEnd.invalid-nativeTextureCaptureBegin.invalid,nativeTextureCaptureEnd.bytes-nativeTextureCaptureBegin.bytes);
 for(unsigned i=0;i<3;++i)fprintf(f,"NATIVE_TEXTURE_CAPTURE_COST outcome=%s calls=%u total_us=%u hash_us=%u open_us=%u read_verify_us=%u overlapping_fields=0 total_is_derived_sum=1 phase_fields_disjoint_within_attempt=1 includes_yields=1 cpu_time=0\n",
     i==0?"hit":i==1?"miss":"invalid",nativeTextureCaptureEnd.cost[i][0]-nativeTextureCaptureBegin.cost[i][0],
     nativeTextureCaptureEnd.cost[i][1]-nativeTextureCaptureBegin.cost[i][1],nativeTextureCaptureEnd.cost[i][2]-nativeTextureCaptureBegin.cost[i][2],
     nativeTextureCaptureEnd.cost[i][3]-nativeTextureCaptureBegin.cost[i][3],nativeTextureCaptureEnd.cost[i][4]-nativeTextureCaptureBegin.cost[i][4]);
#endif
#ifdef SRR2_OLD3DS_COARSE_POOLS
 Old3dsPools::Dump(f);
#endif
#ifdef SRR2_OLD3DS_MAIN_PROFILE
 Old3dsMainProfile::Dump(f,(double)scopeCalibration*1e9/SYSCLOCK_ARM11/4096);
#endif
#if defined(SRR2_OLD3DS_VEHICLE_PROFILE) || defined(SRR2_OLD3DS_NATIVE_BENCH)
 fprintf(f,"NATIVE_VEHICLE_PROFILE measured_route_only=1 sample_ratio=32 phases_nested_in_update=1\n");
 Old3dsVehicleProfile::Dump(f);
#endif
#ifdef SRR2_OLD3DS_SUBMIT_PROFILE
 nativeBenchDumpSubmissionProfile(f);
#endif
 fprintf(f,"NATIVE_BUILD id=%s scenario=%s mode=%s seed=%u step_ms=%u seconds=%u gate=%u selected_render=%u selected_physics=%u aborted=%u detail=%u validate=%u hold=%u render=%u physics=%u record_bytes=%u capacity=%u overwritten=%u scope_pair_ns=%.3f\n",SRR2_OLD3DS_BUILD_ID,portrait?"character-portrait":roadRoute?"L2-road-v2":"L2-drive-v1",replay?"replay":"realtime",seed,step,seconds,0u,0u,0u,aborted,detail,validation,hold,
#ifdef SRR2_OLD3DS_NATIVE_RENDER
 1u,
#else
 0u,
#endif
#ifdef SRR2_OLD3DS_NATIVE_PHYSICS
 1u,
#else
 0u,
#endif
(unsigned)(Capacity*sizeof(Record)),Capacity,overwritten,(double)scopeCalibration*1e9/SYSCLOCK_ARM11/4096);
 fprintf(f,"NATIVE_AUTOMATION diagnostic=%u resident=%u uniforms=%u context_mode=%u source=automation_only clock_complete=%u validation_instrumented=%u gpu_last_is_unassociated=1\n",diagnostic,residentShaders,nativeUniforms,nativeContextMode,done && !aborted,
 0u
 );
 fprintf(f,"NATIVE_GRAPHICS model=%u quality=%u stereo=%u width=%u height=240 target_fps=%u\n",unsigned(Graphics3DS::SystemModel()),Graphics3DS::Quality(),Graphics3DS::Stereo(),Graphics3DS::RenderWidth(),Graphics3DS::TargetFPS());
 fprintf(f,"NATIVE_QUALITY mode=%u draw_distance=%.1f ambient_traffic=%u ambient_pedestrians=%u physics_step_ms=%u audio_predictors=%u particle_emission_scale=%.2f distant_pedestrian_actions=%u flat_lighting=%u shadow_distance=%.1f mission_simulation=full\n",Old3dsPerformance::mode,Old3dsPerformance::Enabled()?Old3dsPerformance::DrawDistance():200.0f,Old3dsPerformance::Enabled()?Old3dsPerformance::TrafficLimit():5u,Old3dsPerformance::Enabled()?Old3dsPerformance::PedestrianLimit():7u,Old3dsPerformance::PhysicsStepMilliseconds(),Old3dsPerformance::ReducedAudioSearch()?3u:8u,Old3dsPerformance::ParticleEmissionScale(),Old3dsPerformance::DistantPedestrianActions(),Old3dsPerformance::FlatLighting(),Old3dsPerformance::Enabled()?0.0f:200.0f);
 fprintf(f,"NATIVE_PORTRAIT angle=%u level=%u performance_capture=0\n",portrait,portraitLevel);
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
 fprintf(f,"NATIVE_CONTEXT_CAPTURE fast=%u fallback=%u words=%u measured_route_only=1\n",contextFastEnd-contextFastBegin,contextFallbackEnd-contextFallbackBegin,contextWordsEnd-contextWordsBegin);
#endif
#ifdef SRR2_OLD3DS_DISTANCE_HAZE
 fprintf(f,"NATIVE_DISTANCE_HAZE enabled=%u builds=%u passes=%u near=%.6f far=%.6f strength=%.3f cached_lut=1 shader_unchanged=1\n",picaDistanceHaze::enabled,picaDistanceHaze::builds,picaDistanceHaze::passes,picaDistanceHaze::nearPlane,picaDistanceHaze::farPlane,picaDistanceHaze::Strength);
#endif
#ifdef SRR2_OLD3DS_CHARACTER_FORM
 fprintf(f,"NATIVE_CHARACTER_FORM enabled=%u groups=%u vertices=%u load_only=1 shader_unchanged=1\n",picaCharacterForm::enabled,picaCharacterForm::groups,picaCharacterForm::vertices);
#endif
 fprintf(f,"NATIVE_PERIOD samples=%u mean_us=%.3f fps=%.6f\n",periodSamples,periodSamples?(double)periodSum*1e6/SYSCLOCK_ARM11/periodSamples:0,periodSum?(double)periodSamples*SYSCLOCK_ARM11/periodSum:0);
 fprintf(f,"NATIVE_LOAD_WINDOW placement_valid=%u boot_to_placement_us=%.3f includes_initialization_and_loading=1 includes_realtime_stability_ms=%u excludes_netloader_transfer=1\n",
     placementTick!=0,placementTick?(placementTick-initializationTick)*1000000.0/SYSCLOCK_ARM11:0,replay?0u:3000u);
 fprintf(f,"NATIVE_ROUTE road=%u points=%u point=%u distance=%.3f recoveries=%u input_only=1\n",roadRoute,roadPointCount,roadPoint,roadDistance,roadRecoveries);
 fprintf(f,"NATIVE_REPLAY drains=%u timeouts=%u reload=%u reload_phase=%u reload_dispatches=%u reload_observed_levels=%u\n",replayDrains,drainTimeouts,reload,reloadPhase,reloadDispatches,reloadObservedLevels);
 fprintf(f,"NATIVE_VERIFY calls=%u failures=%u\n",verifyAll,verifyAllFailures);
 fprintf(f,"NATIVE_COUNTERS 0=timed_scope_calls 1=adpcm_clips 2=adpcm_pcm_samples\n");
 fprintf(f,"NATIVE_SUMMARY frames=%u mean_us=%.3f max_us=%u deadline_misses=%u bookkeeping_us=%.3f complete=%u\n",samples,samples?(double)totalWork*1000000/SYSCLOCK_ARM11/samples:0,maximum,misses,(double)traceTicks*1000000/SYSCLOCK_ARM11,done && !aborted);
 for(unsigned i=0;i<Bins;++i)if(hist[i])fprintf(f,"NATIVE_HIST low_us=%u count=%u\n",i*250,hist[i]);
 for(unsigned i=recordCount>Capacity?recordCount-Capacity:0;i<recordCount;++i){const Record&r=records[i%Capacity];
 fprintf(f,"NATIVE_FRAME native_context=%u route_ms=%u pairs=%u areas=%u entities=%u frame=%u context=%u elapsed=%u real_elapsed=%u period_us=%.3f work_us=%.3f substeps=%u draws=%u vertices=%u linear=%u loading=%u x=%.6f y=%.6f z=%.6f speed=%.5f incar=%u check=%08x behavior=%08x gpu_last_ms=%.4f",r.nativeContext,r.route,r.pairs,r.areas,r.entities,r.frame,r.context,r.elapsed,r.realElapsed,(double)r.period*1e6/SYSCLOCK_ARM11,(double)r.work*1000000/SYSCLOCK_ARM11,r.substeps,r.draws,r.vertices,r.linear,r.loading,r.position[0],r.position[1],r.position[2],r.speed,r.incar,r.check,r.behavior,r.gpu);
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 fprintf(f," bottom_map=%u bottom_draws=%u",r.bottomMapEnabled,r.bottomMapDraws);
#endif
 fprintf(f," heap_used=%u submit_begin=%u submit_end=%u",r.heapUsed,r.presentBegin,r.presentEnd);
 for(unsigned j=0;j<Count;++j)fprintf(f," %s_us=%.3f %s_ex_us=%.3f",names[j],(double)r.stage[j]*1000000/SYSCLOCK_ARM11,names[j],(double)r.exclusive[j]*1000000/SYSCLOCK_ARM11);
 for(unsigned j=0;j<CountC;++j)fprintf(f," count_%u=%u",j,r.count[j]);fprintf(f,"\n");}
 #ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 fprintf(f,"NATIVE_BOTTOM_POLICY mode=%u block_frames=24 randomized=%u policy_seed=%u simulation_rng_unchanged=1\n",bottomMapMode,bottomMapMode==2,bottomMapSeed);
 Old3dsBottomMap::Dump(f);
 #endif
 Old3dsCadence::Dump(f);Old3dsMemoryDump(f);DumpVisual(f);fprintf(f,"NATIVE_CAPTURE_END frames=%u complete=%u\n",samples,done && !aborted);fclose(f);
}
int Input(bool axis,int id,int original){
 if(!enabled||done)return original;if(!placed||warmMs<warmup*1000||done)return 0;
 if(portrait)return 0;
 // Driving-only route: retain the established first 25 seconds of inputs.
 // No exit, walking, return navigation, reset or forced mount during capture.
 if(roadRoute){
  if(routeMs<Old3dsRecurring::stationaryMs)return !axis && id==SDL_CONTROLLER_BUTTON_Y;
  if(axis)return id==SDL_CONTROLLER_AXIS_LEFTX?roadSteer:0;
  if(id==SDL_CONTROLLER_BUTTON_A)return roadGas;
  if(id==SDL_CONTROLLER_BUTTON_B)return roadBrake;
  return 0;
 }
 unsigned t=routeMs;
 if(axis){
  if(id==SDL_CONTROLLER_AXIS_LEFTX && t>=12000 && t<25000)
   return (t/4000)%4==1?2000:((t/4000)%4==3?-2000:0);
  return 0;
 }
 if(id==SDL_CONTROLLER_BUTTON_A)return t<8000 || (t>=11000&&t<20000) || (t>=23000&&t<25000);
 if(id==SDL_CONTROLLER_BUTTON_B)return (t>=8000&&t<11000)||(t>=20000&&t<23000);
 return 0;
}
}
extern "C" int nativeBenchInput(int axis,int id,int original){return NativeBench::Input(axis!=0,id,original);}
extern "C" bool nativeBenchNativeUniforms(){return NativeBench::NativeUniforms();}

#endif
