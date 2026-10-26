#include <p3d/billboardbatch.hpp>
#include <main/graphics3ds.h>
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
#include <p3d/spriteatlas.hpp>
#endif
#include <main/old3dsperformance.h>
#include <main/landmarkvisibility.h>
#include <main/old3dsrecurring.h>
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#endif
#include <main/handheldhudcadence.h>
#include <p3d/fontwork.hpp>
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
#include <pddi/pica/visualstyle.hpp>
#include <pddi/pica/skinning.hpp>
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
#include <pddi/pica/contactshadow.hpp>
#endif
#ifdef SRR2_OLD3DS_GEOMETRY_INPUT
#include <p3d/geometryinput.hpp>
#endif
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <main/commandlineoptions.h>
#include <contexts/context.h>
#include <loading/loadingmanager.h>
#include <worldsim/avatarmanager.h>
#include <worldsim/vehiclecentral.h>
#include <poser/poseengine.hpp>
#include <worldsim/character/character.h>
#include <worldsim/character/charactermanager.h>
#include <worldsim/redbrick/vehicle.h>
#include <mission/gameplaymanager.h>
#include <meta/triggervolumetracker.h>
#include <meta/triggervolume.h>
#include <meta/zoneeventlocator.h>
#include <main/old3dsdiag.h>
#include <main/old3dsexperiment.h>
#include <citro3d.h>
#include <radfile.hpp>
#include <p3d/loadmanager.hpp>
#include <p3d/pointcamera.hpp>
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
#ifdef SRR2_OLD3DS_CHARACTER_OCCLUSION
namespace picaCharacterOcclusion { extern unsigned groups,vertices,misses; }
#endif
#include <presentation/gui/guisystem.h>
#include <presentation/gui/ingame/guimanageringame.h>
#include <roads/road.h>
#include <roads/roadsegment.h>
#include <roads/intersection.h>
#include <render/IntersectManager/IntersectManager.h>
#include <render/RenderManager/RenderManager.h>
#include <render/RenderManager/WorldRenderLayer.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
extern bool g_picaSkinPrepareYield;
extern unsigned g_picaSkinPrepareCheckpoints;
#endif
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
extern bool g_picaNativeContiguous;
#endif
extern bool g_picaNativeTextureDiagnostics;
#include <pddi/pica/texture.hpp>
static picaNativeTextureStats nativeTextureCaptureBegin={},nativeTextureCaptureEnd={};
extern "C" void picaDumpNativeTextures(FILE*);
#include <radnativeseek.hpp>
extern "C" void radLoadDumpWorkerSlices(FILE*);
#endif
extern "C" unsigned g_picaPresentedFrames;
extern bool g_old3dsTrafficFacingOnly;
#ifdef SRR2_OLD3DS_WHEEL_LOD
extern bool g_old3dsWheelBatch;
#endif
extern bool g_old3dsAdpcmPrune,g_old3dsAdpcmVerify,g_old3dsAdpcmFast;
#ifdef SRR2_OLD3DS_PREPARED_AUDIO
extern bool g_old3dsPreparedAudio,g_old3dsPreparedAudioShort;
#endif
extern void old3dsDumpAdpcmVerification(FILE*);
extern unsigned g_old3dsAdpcmFastCalls,g_old3dsAdpcmFastFallbacks,g_old3dsAdpcmBudgetCalls;
extern unsigned g_old3dsAdpcmVerified,g_old3dsAdpcmMismatches,g_old3dsAdpcmVerifyAllocFailures;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
namespace Old3dsPerformance { float visibilityDistance=120.0f; }
#endif
extern bool g_picaParticleTrigCache;
extern "C" void old3dsDumpParticleTrig(FILE*);
namespace NativeBench {
bool detail=false,collecting=false,validation=false,serviceProfile=false;
#ifdef SRR2_OLD3DS_SUBMIT_PROFILE
bool submissionProfile=false;
#endif
static bool vehicleProfile=false;
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
 unsigned route, pairs, areas, entities;
 unsigned skinBudgetFallbacks, skinBudgetVertices; float position[3],speed,gpu; unsigned incar; };
static Record* records=nullptr;
static Record current;
static unsigned hist[Bins],frame,recordCount,overwritten,elapsedMs,routeMs,seconds=25,warmup=8,seed=231171,step=67;
static unsigned samples, misses, maximum,drawStart,vertStart,subStart,pairStart,areaStart,entityStart;
static unsigned skinBudgetStart,skinBudgetVertexStart;
static unsigned entityCalls(){unsigned n=0;for(unsigned i=0;i<O3_CATEGORY_COUNT;++i)n+=g_old3dsEntities[i].calls;return n;}
static bool previousCaptured=false;
static u64 lastBeginTick,periodSum;
static unsigned periodSamples;
static u64 totalWork,traceTicks,scopeCalibration;
static unsigned verifyAll,verifyAllFailures;
static bool hold=false,reload=false,visualCapture=false,afterRouteCapture=false;
static bool sceneAudit=false,placementRelocated=false,placementGround=false;
static rmt::Vector placementPlayer,placementOriginalCar,placementCar;
struct SceneSample { unsigned route,sections,statics,intersects,pending,ground; float x,y,z; };
static SceneSample sceneSamples[64];
static unsigned sceneSampleCount=0,sceneNextMs=0,sceneMissingGround=0;
static unsigned SceneAuditIntervalMs()
{
 // Spread the fixed 64 checks across the requested route, including long runs.
 const unsigned interval=(seconds*1000+63)/64;
 return interval>500?interval:500;
}
static void SampleScene(unsigned route)
{
 if(sceneSampleCount>=64)return;
 SceneSample& sample=sceneSamples[sceneSampleCount++];
 sample.route=route;
 GetRenderManager()->pWorldRenderLayer()->GetLoadedWorldCounts(sample.sections,sample.statics,sample.intersects);
 sample.pending=GetLoadingManager()->GetNumCurrentRequests();
 rmt::Vector position=GetGameplayManager()->GetCurrentVehicle()->GetPosition(),normal,ground;
 sample.x=position.x;sample.y=position.y;sample.z=position.z;
 position.y+=2.0f;bool found=false;
 GetIntersectManager()->FindIntersection(position,found,normal,ground);
 sample.ground=found;
 if(!found)++sceneMissingGround;
}
static float stereoTestSlider=-1.0f;
bool StereoVisualAudit(){return visualCapture;}
float StereoSlider(float physical){return stereoTestSlider>=0.0f?stereoTestSlider:physical;}
// Development-only character inspection. Zero preserves the driving route.
static unsigned portrait=0;
static unsigned portraitLevel=2;
static float portraitDistance=1.0f,portraitBearing=45.0f;
static bool casinoAnchor=false,casinoAnchorPlaced=false,casinoZoneRequested=false;
static rmt::Vector casinoAnchorPosition;
bool LandmarkDiagnostic(){return casinoAnchor;}
static bool portraitNPC=false,portraitTraffic=false,portraitPlayerObserver=false;
static int portraitVehicleIndex=-1;
static Vehicle* portraitVehicle=nullptr;
static char portraitVehicleName[32]={};
static bool portraitActor=false,portraitActorSwapped=false;
static char portraitActorModel[32]="male6";
static int portraitNPCIndex=-1;
static char portraitModel[32]={};
static bool checkpointIO=false;
static unsigned diagnostic=0;
static bool residentShaders=false;
static unsigned loaderSliceUs=0;
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
static_assert(sizeof(names)/sizeof(names[0])==Count,"Every benchmark stage needs one name");
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
bool ResidentShaders(){return residentShaders;}
unsigned LoaderSliceUs(){return loaderSliceUs;}
unsigned CurrentRouteMs(){return elapsedMs;}
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
 static const char actorOption[]="bench_portrait_actor=";
 if(!strncmp(argument,actorOption,sizeof(actorOption)-1)){
  const char* name=argument+sizeof(actorOption)-1;const unsigned length=strlen(name);
  if(!length || length>=sizeof(portraitActorModel))return false;
  for(unsigned i=0;i<length;++i)if(!((name[i]>='a'&&name[i]<='z')||(name[i]>='0'&&name[i]<='9')||name[i]=='_'))return false;
  strcpy(portraitActorModel,name);portraitActor=true;return true;
 }
 if(!strcmp(argument,"bench_portrait_npc=1")){portraitNPC=true;return true;}
 if(!strcmp(argument,"bench_portrait_traffic=1")){portraitTraffic=true;return true;}
 if(!strcmp(argument,"bench_portrait_observer=player")){portraitPlayerObserver=true;return true;}
 static const char portraitDistanceOption[]="bench_portrait_distance=";
 if(!strncmp(argument,portraitDistanceOption,sizeof(portraitDistanceOption)-1)){
  char* end=nullptr;const float value=strtof(argument+sizeof(portraitDistanceOption)-1,&end);
  if(end!=argument+sizeof(portraitDistanceOption)-1 && !*end && std::isfinite(value) && value>=1 && value<=12)portraitDistance=value;
  return true;
 }
 static const char portraitBearingOption[]="bench_portrait_bearing=";
 if(!strncmp(argument,portraitBearingOption,sizeof(portraitBearingOption)-1)){
  char* end=nullptr;const float value=strtof(argument+sizeof(portraitBearingOption)-1,&end);
  if(end!=argument+sizeof(portraitBearingOption)-1 && !*end && std::isfinite(value) && value>=-180 && value<=180)portraitBearing=value;
  return true;
 }
 if(!strncmp(argument,"bench_portrait_model=",21)){
  const char* name=argument+21;const unsigned length=strlen(name);
  if(length>=sizeof(portraitModel))return false;
  for(unsigned i=0;i<length;++i)if(!((name[i]>='a'&&name[i]<='z')||(name[i]>='A'&&name[i]<='Z')||(name[i]>='0'&&name[i]<='9')||name[i]=='_'))return false;
  strcpy(portraitModel,name);return true;
 }
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
 if(!strncmp(argument,"bench_world_vram=",17)){picaVisual::worldVram=atoi(argument+17)!=0;return true;}
 if(!strncmp(argument,"bench_far_backdrop=",19)){picaVisual::farBackdrop=atoi(argument+19)!=0;return true;}
 if(!strncmp(argument,"bench_ground_batch=",19)){picaVisual::groundBatch=atoi(argument+19)!=0;return true;}
 if(!strncmp(argument,"bench_landmark_visibility=",26)){Old3dsLandmark::enabled=atoi(argument+26)!=0;return true;}
 if(!strcmp(argument,"bench_landmark=casino")){casinoAnchor=true;return true;}
 static const char distanceOption[]="bench_draw_distance=";
 if(!strncmp(argument,distanceOption,sizeof(distanceOption)-1)){
  const float value=strtof(argument+sizeof(distanceOption)-1,nullptr);
  if(std::isfinite(value) && value>=120 && value<=240)Old3dsPerformance::visibilityDistance=value;
  return true;
 }
 if(!strncmp(argument,"bench_shadow_staged=",20)){
  picaContact::staged=atoi(argument+20)!=0;return true;
 }
 if(!strncmp(argument,"bench_reflection_outputs=",25)){
  picaNativeEnableSparseReflectionOutputs(atoi(argument+25)!=0);return true;
 }
 if(!strncmp(argument,"bench_visual_features=",22)){
  picaVisual::features=strtoul(argument+22,nullptr,10)&255u;return true;
 }
#endif
 static const char duration[]="bench_seconds=",warm[]="bench_warmup=",keep[]="bench_hold=",
     restart[]="bench_reload=",random[]="bench_seed=",identity[]="bench_trial=";
 if(!strncmp(argument,duration,sizeof(duration)-1))seconds=strtoul(argument+sizeof(duration)-1,nullptr,10);
 else if(!strncmp(argument,warm,sizeof(warm)-1))warmup=strtoul(argument+sizeof(warm)-1,nullptr,10);
 else if(!strncmp(argument,keep,sizeof(keep)-1))hold=atoi(argument+sizeof(keep)-1)!=0;
 else if(!strncmp(argument,restart,sizeof(restart)-1))reload=atoi(argument+sizeof(restart)-1)!=0;
 else if(!strncmp(argument,random,sizeof(random)-1))seed=strtoul(argument+sizeof(random)-1,nullptr,10);
 else if(!strncmp(argument,"bench_quality=",14))Old3dsPerformance::mode=strtoul(argument+14,nullptr,10)%8;
#ifdef SRR2_OLD3DS_PREPARED_AUDIO
 else if(!strncmp(argument,"bench_prepared_audio=",21))g_old3dsPreparedAudio=atoi(argument+21)!=0;
 else if(!strncmp(argument,"bench_prepared_audio_short=",sizeof("bench_prepared_audio_short=")-1))g_old3dsPreparedAudioShort=atoi(argument+sizeof("bench_prepared_audio_short=")-1)!=0;
#endif
#ifdef SRR2_OLD3DS_WHEEL_LOD
 else if(!strncmp(argument,"bench_wheel_batch=",18))g_old3dsWheelBatch=atoi(argument+18)!=0;
#endif
 else if(!strncmp(argument,"bench_adpcm_fast=",17))g_old3dsAdpcmFast=atoi(argument+17)!=0;
 else if(!strncmp(argument,"bench_adpcm_verify=",19))g_old3dsAdpcmVerify=atoi(argument+19)!=0;
 else if(!strncmp(argument,"bench_adpcm_prune=",18))g_old3dsAdpcmPrune=atoi(argument+18)!=0;
 else if(!strncmp(argument,"bench_traffic_facing=",21))g_old3dsTrafficFacingOnly=atoi(argument+21)!=0;
 else if(!strncmp(argument,"bench_vehicle_profile=",22))vehicleProfile=atoi(argument+22)!=0;
 else if(!strncmp(argument,"bench_recurring=",16))Old3dsRecurring::requested=atoi(argument+16)!=0;
#ifdef SRR2_OLD3DS_RECURRING_PROFILE
 else if(!strncmp(argument,"bench_recurring_window=",sizeof("bench_recurring_window=")-1)){
  const char* value=argument+sizeof("bench_recurring_window=")-1;char* end=nullptr;
  const unsigned long start=strtoul(value,&end,10);
  if(value==end || *value<'0' || *value>'9' || *end!=',' || start>=60000)return false;
  value=end+1;const unsigned long finish=strtoul(value,&end,10);
  if(value==end || *value<'0' || *value>'9' || *end || finish>60000 || start>=finish)return false;
  Old3dsRecurring::windowStartMs=unsigned(start);Old3dsRecurring::windowEndMs=unsigned(finish);
 }
 else if(!strncmp(argument,"bench_recurring_drop=",sizeof("bench_recurring_drop=")-1))Old3dsRecurring::focusDropWindow=atoi(argument+sizeof("bench_recurring_drop=")-1)!=0;
#endif
 else if(!strncmp(argument,"bench_stationary_ms=",20))Old3dsRecurring::stationaryMs=strtoul(argument+20,nullptr,10);
 else if(!strncmp(argument,"bench_service_profile=",22))serviceProfile=atoi(argument+22)!=0;
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 else if(!strncmp(argument,"bench_bottom_animation_boost=",sizeof("bench_bottom_animation_boost=")-1))HandheldHudCadence::boostPickupAnimations=atoi(argument+sizeof("bench_bottom_animation_boost=")-1)!=0;
#endif
 else if(!strncmp(argument,"bench_particle_trig_cache=",sizeof("bench_particle_trig_cache=")-1))g_picaParticleTrigCache=atoi(argument+sizeof("bench_particle_trig_cache=")-1)!=0;
#ifdef SRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE
 else if(!strncmp(argument,"bench_native_contiguous=",sizeof("bench_native_contiguous=")-1))g_picaNativeContiguous=atoi(argument+sizeof("bench_native_contiguous=")-1)!=0;
 else if(!strncmp(argument,"bench_native_input_stage=",sizeof("bench_native_input_stage=")-1))radNativeSeek::stageInput=atoi(argument+sizeof("bench_native_input_stage=")-1)!=0;
 else if(!strncmp(argument,"bench_native_seek_lend=",sizeof("bench_native_seek_lend=")-1))radNativeSeek::enabled=atoi(argument+sizeof("bench_native_seek_lend=")-1)!=0;
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
 else if(!strncmp(argument,"bench_skin_prepare_yield=",sizeof("bench_skin_prepare_yield=")-1))g_picaSkinPrepareYield=atoi(argument+sizeof("bench_skin_prepare_yield=")-1)!=0;
#endif
 else if(!strncmp(argument,"bench_after_route_capture=",sizeof("bench_after_route_capture=")-1))afterRouteCapture=atoi(argument+sizeof("bench_after_route_capture=")-1)!=0;
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
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
 else if(!strcmp(key,"vram_capture_promote"))g_picaVramCapturePromote=atoi(value)!=0;
 else if(!strcmp(key,"vram_copy_diagnostics"))g_picaVramCopyDiagnostics=atoi(value)!=0;
#endif
 else if(!strcmp(key,"loader_slice_us")){unsigned valueUs=strtoul(value,nullptr,10);loaderSliceUs=valueUs>=1000 && valueUs<=8000?valueUs:0;}
 else if(!strcmp(key,"prepared_font_glyphs"))p3dFontWork::enabled=atoi(value)!=0;
 #ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 else if(!strcmp(key,"bottom_seed"))bottomMapSeed=strtoul(value,nullptr,10);
 else if(!strcmp(key,"bottom_map"))bottomMapMode=strtoul(value,nullptr,10)%3;
 else if(!strcmp(key,"bottom_cadence"))HandheldHudCadence::cadenceMode=strtoul(value,nullptr,10)%3;
 else if(!strcmp(key,"bottom_minimum_hz"))HandheldHudCadence::minimumHz=strtoul(value,nullptr,10)==8?8:10;
 else if(!strcmp(key,"hud_direct_background"))HandheldHudCadence::directHudBackground=atoi(value)!=0;
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
 else if(!strcmp(key,"after_route_capture"))afterRouteCapture=atoi(value)!=0;
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
 else if(!strcmp(key,"visual_features"))picaVisual::features=strtoul(value,nullptr,10)&255u;
#endif
 else if(!strcmp(key,"portrait_npc"))portraitNPC=atoi(value)!=0;
 else if(!strcmp(key,"portrait"))portrait=strtoul(value,nullptr,10)%6;
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
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
  if(!strncmp(argv[i],"bench_sprite_atlas_mips=",sizeof("bench_sprite_atlas_mips=")-1)){p3dSpriteAtlas::allowMipmaps=atoi(argv[i]+sizeof("bench_sprite_atlas_mips=")-1)!=0;continue;}
  if(!strncmp(argv[i],"bench_sprite_atlas_diagnostics=",sizeof("bench_sprite_atlas_diagnostics=")-1)){p3dSpriteAtlas::diagnostics=atoi(argv[i]+sizeof("bench_sprite_atlas_diagnostics=")-1)!=0;continue;}
  if(!strncmp(argv[i],"bench_sprite_atlas=",sizeof("bench_sprite_atlas=")-1)){p3dSpriteAtlas::enabled=atoi(argv[i]+sizeof("bench_sprite_atlas=")-1)!=0;continue;}
#endif
  static const char checkpointArgument[]="bench_checkpoint_io=";
  if(!strncmp(argv[i],checkpointArgument,sizeof(checkpointArgument)-1)){checkpointIO=atoi(argv[i]+sizeof(checkpointArgument)-1)!=0;continue;}
  if(!strncmp(argv[i],"bench_scene_audit=",18)){sceneAudit=atoi(argv[i]+18)!=0;continue;}
  static const char visualArgument[]="bench_visual_capture=";
  if(!strncmp(argv[i],visualArgument,sizeof(visualArgument)-1)){visualCapture=atoi(argv[i]+sizeof(visualArgument)-1)!=0;continue;}
  static const char portraitArgument[]="bench_portrait=";
  if(!strncmp(argv[i],portraitArgument,sizeof(portraitArgument)-1)){portrait=strtoul(argv[i]+sizeof(portraitArgument)-1,nullptr,10)%6;continue;}
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
  static const char bottomMinimumArgument[]="bench_bottom_minimum_hz=";
  if(!strncmp(argv[i],bottomMinimumArgument,sizeof(bottomMinimumArgument)-1)){HandheldHudCadence::minimumHz=strtoul(argv[i]+sizeof(bottomMinimumArgument)-1,nullptr,10)==8?8:10;continue;}
  static const char bottomCadenceArgument[]="bench_bottom_cadence=";
  if(!strncmp(argv[i],bottomCadenceArgument,sizeof(bottomCadenceArgument)-1)){HandheldHudCadence::cadenceMode=strtoul(argv[i]+sizeof(bottomCadenceArgument)-1,nullptr,10)%3;continue;}
  static const char directBackgroundArgument[]="bench_hud_direct_background=";
  if(!strncmp(argv[i],directBackgroundArgument,sizeof(directBackgroundArgument)-1)){HandheldHudCadence::directHudBackground=atoi(argv[i]+sizeof(directBackgroundArgument)-1)!=0;continue;}
#endif
  if(!strncmp(argv[i],"bench_route=",12)){roadRoute=!strcmp(argv[i]+12,"road");continue;}
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
  static const char capturePromoteArgument[]="bench_vram_capture_promote=",copyDiagnosticsArgument[]="bench_vram_copy_diagnostics=";
  if(!strncmp(argv[i],capturePromoteArgument,sizeof(capturePromoteArgument)-1)){g_picaVramCapturePromote=atoi(argv[i]+sizeof(capturePromoteArgument)-1)!=0;continue;}
  if(!strncmp(argv[i],copyDiagnosticsArgument,sizeof(copyDiagnosticsArgument)-1)){g_picaVramCopyDiagnostics=atoi(argv[i]+sizeof(copyDiagnosticsArgument)-1)!=0;continue;}
#endif
  static const char loaderSliceArgument[]="bench_loader_slice_us=";
  if(!strncmp(argv[i],loaderSliceArgument,sizeof(loaderSliceArgument)-1)){unsigned valueUs=strtoul(argv[i]+sizeof(loaderSliceArgument)-1,nullptr,10);loaderSliceUs=valueUs>=1000 && valueUs<=8000?valueUs:0;continue;}
#ifdef SRR2_OLD3DS_QUAD_STREAM
  static const char billboardCompactArgument[]="bench_billboard_compact=";
  if(!strncmp(argv[i],billboardCompactArgument,sizeof(billboardCompactArgument)-1)){p3dBillboardBatch::compact=atoi(argv[i]+sizeof(billboardCompactArgument)-1)!=0;continue;}
  static const char billboardBatchArgument[]="bench_billboard_batch=";
  if(!strncmp(argv[i],billboardBatchArgument,sizeof(billboardBatchArgument)-1)){p3dBillboardBatch::enabled=atoi(argv[i]+sizeof(billboardBatchArgument)-1)!=0;continue;}
#endif
  static const char preparedFontArgument[]="bench_prepared_font_glyphs=";
  if(!strncmp(argv[i],preparedFontArgument,sizeof(preparedFontArgument)-1)){p3dFontWork::enabled=atoi(argv[i]+sizeof(preparedFontArgument)-1)!=0;continue;}
#ifdef SRR2_OLD3DS_QUAD_STREAM
  static const char fontQuadArgument[]="bench_font_quad_stream=";
  if(!strncmp(argv[i],fontQuadArgument,sizeof(fontQuadArgument)-1)){p3dFontWork::quadStream=atoi(argv[i]+sizeof(fontQuadArgument)-1)!=0;continue;}
#endif
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
  static const char dropProfileArgument[]="bench_main_profile_drop=";
  if(!strncmp(argv[i],dropProfileArgument,sizeof(dropProfileArgument)-1)){Old3dsMainProfile::FocusDropWindow(atoi(argv[i]+sizeof(dropProfileArgument)-1)!=0);continue;}
  static const char mainProfileArgument[]="bench_main_profile=";
  if(!strncmp(argv[i],mainProfileArgument,sizeof(mainProfileArgument)-1)){Old3dsMainProfile::Configure(strtoul(argv[i]+sizeof(mainProfileArgument)-1,nullptr,10)!=0);continue;}
#endif
#ifdef SRR2_OLD3DS_SUBMIT_PROFILE
  static const char submissionProfileArgument[]="bench_submit_profile=";
  if(!strncmp(argv[i],submissionProfileArgument,sizeof(submissionProfileArgument)-1)){submissionProfile=atoi(argv[i]+sizeof(submissionProfileArgument)-1)!=0;continue;}
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
 HandheldHudCadence::enabled=bottomMapMode!=0;
#endif
 const unsigned minimumSeconds=portrait?1u:5u,minimumWarmup=portrait?0u:3u;
 if(seconds<minimumSeconds||seconds>(roadRoute?60u:25u))seconds=25;
 if(warmup<minimumWarmup||warmup>60)warmup=8;if(step<1||step>100)step=67;
 // Use the existing normal heap after libctru startup, not additional BSS.
 records=static_cast<Record*>(calloc(Capacity,sizeof(Record)));
 printf("NATIVE_LAUNCH id=%s mode=%s seconds=%u checkpoint_io=%u\n",runIdentity,replay?"replay":"realtime",seconds,checkpointIO);
 if(!records){enabled=false;aborted=true;done=true;
  printf("NATIVE_AUTOMATION allocation_failed bytes=%u abort=1\n",unsigned(Capacity*sizeof(Record)));return;}
 bool savedDetail=detail;detail=true;collecting=true;u64 cal=svcGetSystemTick();for(unsigned i=0;i<4096;++i){Scope test(Update);}scopeCalibration=svcGetSystemTick()-cal;collecting=false;detail=savedDetail;
 char a[]="skipfe",b[]="notutorial",c[]="L2";
 if(portraitLevel>=1 && portraitLevel<=7)c[1]='0'+portraitLevel;
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
 if(portrait && portraitActor && !portraitActorSwapped){
  // A photo actor uses the existing costume loader and generic pedestrian rig.
  // It cannot deactivate with the ambient pedestrian population. Route tests
  // never request this diagnostic override.
  GetCharacterManager()->SwapData(avatar->GetCharacter(),portraitActorModel,"npd");
  portraitActorSwapped=true;stableMs=0;return dt;
 }
 // Let the normal pedestrian population activate, then take a short portrait
 // before a walking subject leaves the player's activation radius.
 if(!replay && stableMs<(portraitNPC || portraitTraffic?9000u:3000u))return dt;
 if(casinoAnchor && !casinoAnchorPlaced){
  if(portraitLevel!=3){aborted=done=true;return dt;}
  const rmt::Vector hint(-175.0f,-40.0f,-110.0f);
  RoadSegment* segment=nullptr;float distance=0;
  GetIntersectManager()->FindClosestRoad(hint,100,segment,distance);
  if(!segment){aborted=done=true;return dt;}
  rmt::Vector start,end;segment->GetPosition(0,.5f,&start);segment->GetPosition(1,.5f,&end);
  const rmt::Vector edge=end-start;const float length=edge.MagnitudeSqr();
  float fraction=length>0?(hint-start).Dot(edge)/length:.5f;
  if(fraction<0)fraction=0;if(fraction>1)fraction=1;
  segment->GetPosition(fraction,.5f,&casinoAnchorPosition);
  rmt::Vector heading(-387.0f-casinoAnchorPosition.x,0,-200.0f-casinoAnchorPosition.z);heading.Normalize();
  Vehicle* car=GetGameplayManager()->GetCurrentVehicle();
  car->SetInitialPositionGroundOffsetAutoAdjust(&casinoAnchorPosition);
  car->mResetFacingRadians=std::atan2(heading.x,heading.z);
  car->Reset(true,false);
  GetAvatarManager()->PutCharacterInCar(avatar->GetCharacter(),car);avatar->SetCameraTargetToVehicle(true);
  // Teleport setup can skip the thin streaming entry volumes crossed by a
  // normal drive. Activate the nearest ordinary zone event that loads R2.
  ZoneEventLocator* zone=nullptr;float nearest=1e30f;
  TriggerVolumeTracker* tracker=GetTriggerVolumeTracker();
  for(unsigned i=0;i<tracker->GetNumTriggers();++i){
   TriggerVolume* volume=tracker->GetTrigger(i);auto* locator=volume->GetLocator();
   if(!locator || locator->GetDataType()!=LocatorType::DYNAMIC_ZONE)continue;
   auto* candidate=static_cast<ZoneEventLocator*>(locator);if(candidate->IsInteriorLoad())continue;
   bool contains=false;for(int j=0;j<candidate->GetNumLoadZones();++j)contains|=strstr(candidate->GetLoadZone(j),"l3r2")!=nullptr;
   if(!contains)continue;const float distance=(volume->GetPosition()-casinoAnchorPosition).MagnitudeSqr();
   if(distance<nearest){nearest=distance;zone=candidate;}
  }
  if(!zone){aborted=done=true;return dt;}
  zone->Trigger(0,true);casinoZoneRequested=true;
  casinoAnchorPlaced=true;stableMs=0;return dt;
 }
 // A level's default car can be parked outside the mission's loaded sectors.
 // Mounting it would teleport the player into an unloaded world (L6 starts the
 // player near the comic shop, but parks bart_v at Krustylu Studios).
 // Keep the established nearby-car routes; relocate only a remote parked car
 // onto the road nearest the already loaded player. This is test setup only.
 Vehicle* setupCar=GetGameplayManager()->GetCurrentVehicle();
 avatar->GetPosition(placementPlayer);placementOriginalCar=setupCar->GetPosition();
 if(!casinoAnchor && (placementOriginalCar-placementPlayer).MagnitudeSqr()>40.0f*40.0f){
  RoadSegment* segment=nullptr;float distance=0;
  GetIntersectManager()->FindClosestRoad(placementPlayer,30.0f,segment,distance);
  if(!segment){aborted=done=true;return dt;}
  rmt::Vector a,b,facing;segment->GetPosition(0,.5f,&a);segment->GetPosition(1,.5f,&b);
  const rmt::Vector edge=b-a;const float length=edge.MagnitudeSqr();
  float fraction=length>0?(placementPlayer-a).Dot(edge)/length:.5f;
  if(fraction<0)fraction=0;if(fraction>1)fraction=1;
  rmt::Vector position;segment->GetPosition(fraction,.5f,&position);
  avatar->GetCharacter()->GetFacing(facing);
  const float sign=edge.x*facing.x+edge.z*facing.z>=0?1.0f:-1.0f;
  setupCar->SetInitialPositionGroundOffsetAutoAdjust(&position);
  setupCar->mResetFacingRadians=std::atan2(sign*edge.x,sign*edge.z);
  placementRelocated=true;
 }
 setupCar->Reset(true,false);
 placementCar=setupCar->GetPosition();
 // Fail before capture if the route has no loaded collision surface below it.
 rmt::Vector groundProbe=placementCar,groundNormal,groundPosition;groundProbe.y+=2.0f;
 GetIntersectManager()->FindIntersection(groundProbe,placementGround,groundNormal,groundPosition);
 if(!placementGround){aborted=done=true;return dt;}
 GetAvatarManager()->PutCharacterInCar(avatar->GetCharacter(),GetGameplayManager()->GetCurrentVehicle());
 avatar->SetCameraTargetToVehicle(true);
 GetSuperCamManager()->GetSCC(0)->SelectSuperCam(SuperCam::FAR_FOLLOW_CAM,SuperCamCentral::CUT|SuperCamCentral::FORCE);
 if(portrait){
  GetAvatarManager()->PutCharacterOnGround(avatar->GetCharacter(),GetGameplayManager()->GetCurrentVehicle());
  avatar->SetCameraTargetToCharacter(true);
  Character* character=avatar->GetCharacter();
  rmt::Vector position;character->GetPosition(position);
  character->RelocateAndReset(position,0.0f);
 if(portrait && portraitTraffic && portraitVehicleIndex<0){
  rmt::Vector player;avatar->GetPosition(player);float nearest=35.0f*35.0f;
  for(int i=0;i<GetVehicleCentral()->GetMaxActiveVehicles();++i){
   Vehicle* candidate=GetVehicleCentral()->GetVehicle(i);
   if(!candidate || candidate->mVehicleType!=VT_TRAFFIC || candidate==GetGameplayManager()->GetCurrentVehicle())continue;
   if(portraitModel[0] && strcmp(candidate->GetName(),portraitModel))continue;
   const float distance=(candidate->GetPosition()-player).MagnitudeSqr();
   if(distance<nearest){nearest=distance;portraitVehicleIndex=i;portraitVehicle=candidate;}
  }
  if(portraitVehicleIndex<0){if(stableMs>20000u)aborted=done=true;return dt;}
  std::snprintf(portraitVehicleName,sizeof(portraitVehicleName),"%s",GetVehicleCentral()->GetVehicle(portraitVehicleIndex)->GetName());
 }
  if(portraitNPC){
   float nearest=1000000.0f;
   for(int i=0;i<CharacterManager::GetMaxCharacters();++i){
    Character* candidate=GetCharacterManager()->GetCharacter(i);
    if(!candidate || candidate==avatar->GetCharacter() || !candidate->IsVisible() ||
       candidate->GetRole()!=Character::ROLE_PEDESTRIAN)continue;
    if(portraitModel[0] && strcmp(GetCharacterManager()->GetModelName(candidate),portraitModel))continue;
    rmt::Vector location;candidate->GetPosition(location);
    const float distance=(location-position).MagnitudeSqr();
    if(distance<nearest){nearest=distance;character=candidate;portraitNPCIndex=i;}
   }
   character->GetPosition(position);
  }
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
 // Portraits follow the existing subject rather than letting a walking NPC
 // leave a fixed camera behind. This diagnostic view is never performance proof.
 if(portrait && placed && avatar && avatar->GetCharacter()){
  Character* subject=portraitNPCIndex>=0?GetCharacterManager()->GetCharacter(portraitNPCIndex):avatar->GetCharacter();
  StaticCam* camera=static_cast<StaticCam*>(GetSuperCamManager()->GetSCC(0)->GetSuperCam(SuperCam::STATIC_CAM));
  if(subject && camera){
   rmt::Vector position,facing;subject->GetPosition(position);subject->GetFacing(facing);
   // CharacterRenderable culls centers within near+1.5m. Stay outside that
   // gameplay safety rule and frame the subject with FOV rather than proximity.
   const float front=portrait==3?0.0f:(portrait==2?-3.6f:3.6f),side=portrait==3?3.6f:0.0f;
   camera->SetTarget(subject->GetTarget());camera->SetTracking(false);
   camera->SetTargetOffset(rmt::Vector(position.x,position.y+1.05f,position.z));
   camera->SetPosition(rmt::Vector(position.x+front*facing.x+side*facing.z,position.y+1.20f,
                                  position.z+front*facing.z-side*facing.x));
   camera->SetMaxFOV(1.12f);camera->SetFOV(1.12f);
   if(portrait>=4 && GetGameplayManager()->GetCurrentVehicle()){
    if(portraitTraffic && portraitVehicle)portraitVehicleIndex=GetVehicleCentral()->GetVehicleId(portraitVehicle,false);
    Vehicle* car=portraitTraffic?(portraitVehicleIndex>=0?GetVehicleCentral()->GetVehicle(portraitVehicleIndex):nullptr):
        GetGameplayManager()->GetCurrentVehicle();
    if(!car || (portraitTraffic && strcmp(car->GetName(),portraitVehicleName))){aborted=done=true;return dt;}
    const rmt::Vector location=car->GetPosition(),direction=car->GetFacing();
    camera->SetTarget(car);
    camera->SetTargetOffset(rmt::Vector(location.x,location.y+.65f,location.z));
    const float radians=portraitBearing*0.017453292519943295f;
    const float front=6.363961030678928f*portraitDistance*std::cos(radians);
    const float side=6.363961030678928f*portraitDistance*std::sin(radians);
    camera->SetPosition(rmt::Vector(location.x+front*direction.x+side*direction.z,location.y+2.3f,
                                   location.z+front*direction.z-side*direction.x));
    if(portraitPlayerObserver){
     rmt::Vector observer;avatar->GetCharacter()->GetPosition(observer);
     camera->SetPosition(rmt::Vector(observer.x,observer.y+1.5f,observer.z));
    }
    camera->SetMaxFOV(1.05f);camera->SetFOV(1.05f);
    if(portrait==5){
     camera->SetTargetOffset(rmt::Vector(location.x+25*direction.x,location.y+2.0f,location.z+25*direction.z));
     camera->SetPosition(rmt::Vector(location.x-3*direction.x,location.y+3.0f,location.z-3*direction.z));
     camera->SetMaxFOV(1.35f);camera->SetFOV(1.35f);
    }
    if(casinoAnchor){
     camera->SetTargetOffset(rmt::Vector(-387.0f,-20.0f,-200.0f));
     camera->SetPosition(rmt::Vector(casinoAnchorPosition.x,casinoAnchorPosition.y+3.0f,casinoAnchorPosition.z));
     camera->SetMaxFOV(1.15f);camera->SetFOV(1.15f);
    }
   }
  }
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
 HandheldHudCadence::enabled=bottomMapMode==1 || (bottomMapMode==2 && (bottomChoice&1));
 current.bottomMapEnabled=HandheldHudCadence::enabled;bottomMapStart=HandheldHudCadence::DrawCount();
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
 if(sceneAudit && elapsedMs>=sceneNextMs){SampleScene(elapsedMs);sceneNextMs=elapsedMs+SceneAuditIntervalMs();}
 routeMs=elapsedMs;elapsedMs+=dt;collecting=true;top=0;behaviorHash=2166136261u;
 Checkpoint("enter",-1);
 pairStart=g_old3dsCollisionPairs;areaStart=g_old3dsPhysicsAreas;entityStart=entityCalls();
 current.presentBegin=g_picaPresentedFrames;
 drawStart=g_old3dsDrawCalls;vertStart=g_old3dsDrawVerts;subStart=g_old3dsPhysicsSubsteps;
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
 skinBudgetStart=g_picaSkinMetrics.fallback[picaSkin::BudgetLimit];
 skinBudgetVertexStart=g_picaSkinBudgetFallbackVertices;
#endif
#ifdef SRR2_OLD3DS_MAIN_PROFILE
 Old3dsMainProfile::BeginFrame(frame,collecting,routeMs);
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
 #ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
 current.skinBudgetFallbacks=g_picaSkinMetrics.fallback[picaSkin::BudgetLimit]-skinBudgetStart;
 current.skinBudgetVertices=g_picaSkinBudgetFallbackVertices-skinBudgetVertexStart;
#endif
 current.work=(unsigned)work;current.context=context;current.draws=g_old3dsDrawCalls-drawStart;current.vertices=g_old3dsDrawVerts-vertStart;
 // Heap walks are intrusive on the fragmented gameplay heap. Timing builds
 // leave this field unmeasured; dedicated allocation diagnostics retain peaks.
#ifdef SRR2_OLD3DS_MEMORY_DIAG
 current.heapUsed=mallinfo().uordblks;
#else
 current.heapUsed=0;
#endif
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 current.bottomMapDraws=HandheldHudCadence::DrawCount()-bottomMapStart;
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
static void DumpVisualEye(FILE* f,gfx3dSide_t eye,const char* tag,gfxScreen_t screen=GFX_TOP)
{
 u16 width=0,height=0;
 const GSPGPU_FramebufferFormat format=gfxGetScreenFormat(screen);
 unsigned char* pixels=gfxGetFramebuffer(screen,eye,&width,&height);
 const unsigned bytes=unsigned(width)*height*3;
 const bool valid=pixels && format==GSP_BGR8_OES && width==240 && (screen==GFX_BOTTOM?height==320:(height==400 || height==800)) &&
     R_SUCCEEDED(GSPGPU_InvalidateDataCache(pixels,bytes));
 fprintf(f,"%s enabled=1 valid=%u screen=%s raw_width=%u raw_height=%u format=%u back_buffer=1 age_unknown=1 after_route=1 performance_capture=0\n",tag,valid,screen==GFX_BOTTOM?"bottom":"top",width,height,unsigned(format));
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
 // A post-route photo does not enable the in-route stereo command observer.
 // Refuse that separate capture while benchmark clocks are still collecting.
 if(!visualCapture && (!afterRouteCapture || collecting))return;
 C3D_FrameSync();
 DumpVisualEye(f,GFX_LEFT,"NATIVE_VISUAL");
 if(Graphics3DS::Stereo())DumpVisualEye(f,GFX_RIGHT,"NATIVE_VISUAL_RIGHT");
 DumpVisualEye(f,GFX_LEFT,"NATIVE_VISUAL_BOTTOM",GFX_BOTTOM);
}
extern "C" void picaDumpStereoDiagnostics(FILE*);
void Dump(){if(!enabled)return;
 radNativeSeek::Drain();
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
 fprintf(f,"NATIVE_STEREO_TEST slider_override=%.4f command_audit=%u after_route_framebuffer=%u performance_capture=%u\n",stereoTestSlider,visualCapture,afterRouteCapture,!visualCapture && stereoTestSlider<0.0f);
 fprintf(f,"NATIVE_FUSED_TRIG enabled=1 nearest_only=1 bounded_radians=8192 per_call_counters=0 runtime_selector=0\n");
 old3dsDumpAdpcmVerification(f);
 old3dsDumpParticleTrig(f);
 fprintf(f,"NATIVE_ADPCM_FAST enabled=%u calls=%u fallbacks=%u predictor_order_unchanged=%u budget_calls=%u budget_predictors=3 callbacks_deferred=0\n",g_old3dsAdpcmFast,g_old3dsAdpcmFastCalls,g_old3dsAdpcmFastFallbacks,g_old3dsAdpcmBudgetCalls==0,g_old3dsAdpcmBudgetCalls);
 fprintf(f,"NATIVE_ADPCM_VERIFY enabled=%u verified=%u mismatches=%u allocation_failures=%u performance_capture=%u\n",g_old3dsAdpcmVerify,g_old3dsAdpcmVerified,g_old3dsAdpcmMismatches,g_old3dsAdpcmVerifyAllocFailures,!g_old3dsAdpcmVerify);
 fprintf(f,"NATIVE_ADPCM_PRUNE enabled=%u exact_predictor_order=1 callbacks_deferred=0\n",g_old3dsAdpcmPrune);
 fprintf(f,"NATIVE_TRAFFIC_FACING enabled=%u\n",g_old3dsTrafficFacingOnly);
 fprintf(f,"NATIVE_SERVICE_PROFILE enabled=%u in_memory=1 post_capture_dump=1\n",serviceProfile);
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
 fprintf(f,"NATIVE_SKIN_LOAD_SLICES enabled=%u checkpoints=%u vertex_interval=64 whole_session=1 unchanged_vertex_bytes=1\n",g_picaSkinPrepareYield,g_picaSkinPrepareCheckpoints);
#endif
 radLoadDumpWorkerSlices(f);
 radNativeSeek::Dump(f);
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
 fprintf(f,"NATIVE_SPRITE_ATLAS enabled=%u loaded=%u applied=%u batched=%u avoided_tile_draws=%u source_geometry_unchanged=1\n",p3dSpriteAtlas::enabled,p3dSpriteAtlas::loaded,p3dSpriteAtlas::applied,p3dSpriteAtlas::batched,p3dSpriteAtlas::avoided);
 fprintf(f,"NATIVE_SPRITE_ATLAS_REJECTIONS textures=%u shape=%u tile=%u format=%u\n",p3dSpriteAtlas::texturesLoaded,p3dSpriteAtlas::rejectShape,p3dSpriteAtlas::rejectTile,p3dSpriteAtlas::rejectFormat);
 fprintf(f,"NATIVE_SPRITE_ATLAS_MIPS enabled=%u pixel_review_required=%u\n",p3dSpriteAtlas::allowMipmaps,p3dSpriteAtlas::allowMipmaps);
 p3dSpriteAtlas::DumpSprites(f);
#endif
 fprintf(f,"NATIVE_RUN id=%s warmup_seconds=%u\n",runIdentity,warmup);
 fprintf(f,"NATIVE_PLACEMENT level=%u mission=%u relocated=%u ground=%u player_x=%.5f player_y=%.5f player_z=%.5f original_car_x=%.5f original_car_y=%.5f original_car_z=%.5f car_x=%.5f car_y=%.5f car_z=%.5f\n",unsigned(GetGameplayManager()->GetCurrentLevelIndex())+1,unsigned(GetGameplayManager()->GetCurrentMissionIndex()),placementRelocated,placementGround,placementPlayer.x,placementPlayer.y,placementPlayer.z,placementOriginalCar.x,placementOriginalCar.y,placementOriginalCar.z,placementCar.x,placementCar.y,placementCar.z);
 fprintf(f,"NATIVE_SCENE_AUDIT enabled=%u samples=%u missing_ground=%u performance_capture=%u interval_ms=%u first_route_ms=%u last_route_ms=%u requested_ms=%u\n",sceneAudit,sceneSampleCount,sceneMissingGround,!sceneAudit,SceneAuditIntervalMs(),sceneSampleCount?sceneSamples[0].route:0,sceneSampleCount?sceneSamples[sceneSampleCount-1].route:0,seconds*1000);
 for(unsigned i=0;i<sceneSampleCount;++i){const SceneSample& s=sceneSamples[i];
  fprintf(f,"NATIVE_SCENE route_ms=%u sections=%u statics=%u intersects=%u pending=%u ground=%u x=%.5f y=%.5f z=%.5f\n",s.route,s.sections,s.statics,s.intersects,s.pending,s.ground,s.x,s.y,s.z);
 }

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
 fprintf(f,"NATIVE_BUILD id=%s scenario=%s mode=%s seed=%u step_ms=%u seconds=%u gate=%u selected_render=%u selected_physics=%u aborted=%u detail=%u validate=%u hold=%u render=%u physics=%u record_bytes=%u capacity=%u overwritten=%u scope_pair_ns=%.3f\n",SRR2_OLD3DS_BUILD_ID,portrait?"character-portrait":roadRoute?"loaded-level-road-v3":"loaded-level-drive-v2",replay?"replay":"realtime",seed,step,seconds,0u,0u,0u,aborted,detail,validation,hold,
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
#ifdef SRR2_OLD3DS_QUAD_STREAM
 fprintf(f,"NATIVE_BILLBOARD_BATCH enabled=%u compact=%u minimum_groups=%u batches=%u groups=%u avoided_draws=%u reservation_failures=%u scope=whole_session sorted_order_retained=1\n",p3dBillboardBatch::enabled,p3dBillboardBatch::compact,p3dBillboardBatch::MinimumGroups(),p3dBillboardBatch::batches,p3dBillboardBatch::groups,p3dBillboardBatch::avoided,p3dBillboardBatch::reservationFailures);
#endif
 fprintf(f,"NATIVE_FONT_WORK enabled=%u prepared_lines=%u prepared_glyphs=%u long_lines=%u capacity=%u same_vertices=1 same_texture_order=1 same_spacing_pass=1 whole_session=1\n",p3dFontWork::enabled,p3dFontWork::preparedLines,p3dFontWork::preparedGlyphs,p3dFontWork::longLines,p3dFontWork::Capacity);
 fprintf(f,"NATIVE_FONT_QUADS enabled=%u batches=%u quads=%u fallback_reservations=%u same_expanded_triangles=1 whole_session=1\n",p3dFontWork::quadStream,p3dFontWork::quadBatches,p3dFontWork::quadGlyphs,p3dFontWork::quadFallbacks);
 fprintf(f,"NATIVE_LOADER_SLICE override_us=%u gameplay_only=1 loading_policy_unchanged=1 geometry_io_unchanged=1\n",loaderSliceUs);
 fprintf(f,"NATIVE_GRAPHICS model=%u quality=%u stereo=%u width=%u height=240 target_fps=%u\n",unsigned(Graphics3DS::SystemModel()),Graphics3DS::Quality(),Graphics3DS::Stereo(),Graphics3DS::RenderWidth(),Graphics3DS::TargetFPS());
 fprintf(f,"NATIVE_QUALITY mode=%u draw_distance=%.1f ambient_traffic=%u ambient_pedestrians=%u physics_step_ms=%u audio_predictors=%u particle_emission_scale=%.2f distant_pedestrian_actions=%u flat_lighting=%u shadow_distance=%.1f mission_simulation=full\n",Old3dsPerformance::mode,Old3dsPerformance::Enabled()?Old3dsPerformance::DrawDistance():200.0f,Old3dsPerformance::Enabled()?Old3dsPerformance::TrafficLimit():5u,Old3dsPerformance::Enabled()?Old3dsPerformance::PedestrianLimit():7u,Old3dsPerformance::PhysicsStepMilliseconds(),Old3dsPerformance::ReducedAudioSearch()?3u:8u,Old3dsPerformance::ParticleEmissionScale(),Old3dsPerformance::DistantPedestrianActions(),Old3dsPerformance::FlatLighting(),Old3dsPerformance::Enabled()?0.0f:200.0f);
 fprintf(f,"NATIVE_LANDMARK requested=%u placed=%u zone_requested=%u x=%.5f y=%.5f z=%.5f visibility_distance=%.1f temporary_setup_only=1\n",casinoAnchor,casinoAnchorPlaced,casinoZoneRequested,casinoAnchorPosition.x,casinoAnchorPosition.y,casinoAnchorPosition.z,Old3dsPerformance::DrawDistance());
 if(casinoAnchor && placed){
  tPointCamera* camera=GetSuperCamManager()->GetSCC(0)->GetCamera();
  const rmt::Vector position=camera->GetPosition(),target=camera->GetTarget();
  const rmt::Vector car=GetGameplayManager()->GetCurrentVehicle()->GetPosition();
  fprintf(f,"NATIVE_LANDMARK_CAMERA x=%.5f y=%.5f z=%.5f target_x=%.5f target_y=%.5f target_z=%.5f car_x=%.5f car_y=%.5f car_z=%.5f\n",position.x,position.y,position.z,target.x,target.y,target.z,car.x,car.y,car.z);
 }
 fprintf(f,"NATIVE_PORTRAIT angle=%u level=%u distance_multiplier=%.3f bearing_degrees=%.3f player_observer=%u performance_capture=0\n",portrait,portraitLevel,portraitDistance,portraitBearing,portraitPlayerObserver);
 if(portrait && placed){
  tPointCamera* camera=GetSuperCamManager()->GetSCC(0)->GetCamera();
  const rmt::Vector position=camera->GetPosition(),target=camera->GetTarget();
  fprintf(f,"NATIVE_PORTRAIT_CAMERA x=%.6f y=%.6f z=%.6f target_x=%.6f target_y=%.6f target_z=%.6f\n",position.x,position.y,position.z,target.x,target.y,target.z);
 }
 if(portraitTraffic){
  if(portraitVehicle)portraitVehicleIndex=GetVehicleCentral()->GetVehicleId(portraitVehicle,false);
  Vehicle* subject=portraitVehicleIndex>=0?GetVehicleCentral()->GetVehicle(portraitVehicleIndex):nullptr;
  const bool found=subject && !strcmp(subject->GetName(),portraitVehicleName);
  for(int i=0;i<GetVehicleCentral()->GetMaxActiveVehicles();++i){
   Vehicle* vehicle=GetVehicleCentral()->GetVehicle(i);
   if(!vehicle || vehicle->mVehicleType!=VT_TRAFFIC)continue;
   const rmt::Vector position=vehicle->GetPosition();
   fprintf(f,"NATIVE_TRAFFIC_HEIGHT model=%s index=%d x=%.6f y=%.6f z=%.6f rest_height=%.6f",
       vehicle->GetName(),i,position.x,position.y,position.z,vehicle->GetRestHeightAboveGround());
   if(vehicle->mPoseEngine){poser::Pose* pose=vehicle->mPoseEngine->GetPose();
    for(unsigned wheel=0;wheel<4;++wheel){int joint=vehicle->mWheelToJointIndexMapping[wheel];
     if(joint>=0 && joint<pose->GetJointCount()){
      const rmt::Vector p=pose->GetJoint(joint)->GetWorldTranslation();
      fprintf(f," wheel%u_x=%.6f wheel%u_y=%.6f wheel%u_z=%.6f wheel%u_radius_estimate=%.6f",wheel,p.x,wheel,p.y,wheel,p.z,wheel,vehicle->GetRestHeightAboveGround()+vehicle->mSuspensionRestPoints[0].y);
     }
    }
   }
   fprintf(f,"\n");
  }
  fprintf(f,"NATIVE_TRAFFIC_PORTRAIT requested=1 found=%u index=%d model=%s selected_model=%s requested_model=%s performance_capture=0\n",
      found,portraitVehicleIndex,found?subject->GetName():"none",portraitVehicleName,portraitModel[0]?portraitModel:"nearest");
 }
 fprintf(f,"NATIVE_PORTRAIT_SUBJECT npc_requested=%u npc_index=%d actor_override=%s\n",portraitNPC,portraitNPCIndex,portraitActor?portraitActorModel:"none");
 if(GetGameplayManager()->GetCurrentVehicle())fprintf(f,"NATIVE_CURRENT_VEHICLE name=%s\n",GetGameplayManager()->GetCurrentVehicle()->GetName());
 if(portrait && placed){
  Character* subject=portraitNPCIndex>=0?GetCharacterManager()->GetCharacter(portraitNPCIndex):GetAvatarManager()->GetAvatarForPlayer(0)->GetCharacter();
  tPointCamera* camera=GetSuperCamManager()->GetSCC(0)->GetCamera();
  if(subject && camera){rmt::Vector position;subject->GetPosition(position);
   fprintf(f,"NATIVE_PORTRAIT_GEOMETRY model=%s distance=%.4f near=%.4f visible=%u performance_capture=0\n",
    GetCharacterManager()->GetModelName(subject),(position-camera->GetPosition()).Magnitude(),camera->GetNearPlane(),subject->IsVisible());}
 }
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
 unsigned ordinaryOutputs=0,reflectedOutputs=0,sparseOutputs=0;
 picaNativeReflectionOutputStats(&ordinaryOutputs,&reflectedOutputs,&sparseOutputs);
 fprintf(f,"NATIVE_REFLECTION_OUTPUTS sparse=%u ordinary_changes=%u reflective_changes=%u scope=whole_session\n",
     sparseOutputs,ordinaryOutputs,reflectedOutputs);
 picaNativeReflectionEntryStats(&ordinaryOutputs,&reflectedOutputs,&sparseOutputs);
 fprintf(f,"NATIVE_REFLECTION_ENTRIES enabled=%u ordinary_changes=%u reflective_changes=%u scope=whole_session\n",
     sparseOutputs,ordinaryOutputs,reflectedOutputs);
 fprintf(f,"NATIVE_LANDMARK_POLICY enabled=%u selected=%u root_placed=%u root_fallback=%u visible_entities=%u profile_records=%u scope=whole_session global_distance_unchanged=1 collision_bounds_unchanged=1\n",Old3dsLandmark::enabled,Old3dsLandmark::selected,Old3dsLandmark::rootPlaced,Old3dsLandmark::rootFallback,Old3dsLandmark::visible,Old3dsLandmark::ProfileCache().count);
 fprintf(f,"NATIVE_FAR_BACKDROP enabled=%u loaded=%u drawn=%u scope=whole_session new_textures=0 new_scene_passes=0\n",picaVisual::farBackdrop,picaVisual::farBackdropLoads,picaVisual::farBackdropDraws);
 fprintf(f,"NATIVE_WORLD_MATERIAL_BATCH enabled=%u load_callbacks=%u scope=whole_session extra_scene_passes=0\n",picaVisual::groundBatch,picaVisual::groundBatchNames);
 fprintf(f,"NATIVE_VISUAL_FEATURES mask=%u load_prepared_form=1 reflection_draws_whole_session=%u scenery_draws_whole_session=%u\n",
     picaVisual::features,picaVisual::reflectionDraws,picaVisual::sceneryDraws);
 fprintf(f,"NATIVE_CONTACT_SHADOWS enabled=%u queued=%u batches=%u instances=%u overflow=%u command_skips=%u scope=whole_session cpu_scene_walks=0 new_ground_rays=0\n",
     picaVisual::Enabled(picaVisual::ContactShadows),picaContact::queued,picaContact::batches,picaContact::drawn,picaContact::overflow,picaContact::commandSkips);
 fprintf(f,"NATIVE_CONTACT_STREAM staged=%u vertices=%u fallbacks=%u scope=whole_session\n",
     picaContact::staged,picaContact::stagedVertices,picaContact::stagedFallbacks);
#endif
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
 fprintf(f,"NATIVE_CONTEXT_CAPTURE fast=%u fallback=%u words=%u measured_route_only=1\n",contextFastEnd-contextFastBegin,contextFallbackEnd-contextFallbackBegin,contextWordsEnd-contextWordsBegin);
#endif
#ifdef SRR2_OLD3DS_DISTANCE_HAZE
 fprintf(f,"NATIVE_DISTANCE_HAZE enabled=%u builds=%u passes=%u near=%.6f far=%.6f strength=%.3f cached_lut=1 shader_unchanged=1\n",picaDistanceHaze::enabled,picaDistanceHaze::builds,picaDistanceHaze::passes,picaDistanceHaze::nearPlane,picaDistanceHaze::farPlane,picaVisual::Enabled(picaVisual::Atmosphere)?picaDistanceHaze::ImprovedStrength:picaDistanceHaze::Strength);
#endif
#ifdef SRR2_OLD3DS_CHARACTER_FORM
 fprintf(f,"NATIVE_CHARACTER_FORM enabled=%u groups=%u vertices=%u load_only=1 shader_unchanged=1\n",picaCharacterForm::enabled,picaCharacterForm::groups,picaCharacterForm::vertices);
#endif
#ifdef SRR2_OLD3DS_CHARACTER_OCCLUSION
 fprintf(f,"NATIVE_CHARACTER_OCCLUSION enabled=%u groups=%u vertices=%u misses=%u load_only=1\n",
  picaVisual::Enabled(picaVisual::Occlusion),picaCharacterOcclusion::groups,picaCharacterOcclusion::vertices,picaCharacterOcclusion::misses);
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
 fprintf(f," skin_budget_groups=%u skin_budget_vertices=%u",r.skinBudgetFallbacks,r.skinBudgetVertices);
 for(unsigned j=0;j<Count;++j)fprintf(f," %s_us=%.3f %s_ex_us=%.3f",names[j],(double)r.stage[j]*1000000/SYSCLOCK_ARM11,names[j],(double)r.exclusive[j]*1000000/SYSCLOCK_ARM11);
 for(unsigned j=0;j<CountC;++j)fprintf(f," count_%u=%u",j,r.count[j]);fprintf(f,"\n");}
 #ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 fprintf(f,"NATIVE_BOTTOM_POLICY mode=%u block_frames=24 randomized=%u policy_seed=%u simulation_rng_unchanged=1\n",bottomMapMode,bottomMapMode==2,bottomMapSeed);
 HandheldHudCadence::Dump(f);
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
