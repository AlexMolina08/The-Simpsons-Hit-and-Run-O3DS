#include <main/graphics3ds.h>
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
#include <p3d/spriteatlas.hpp>
#endif
#include <main/old3dsperformance.h>
#include <main/landmarkvisibility.h>
#include <main/old3dsrecurring.h>
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
#include <main/worldstressplan.h>
#include <main/worldinteriorstress.h>
#include <new>
#include <main/worldstreaming.h>
#include <main/worldresidency.h>
#include <render/Loaders/AllWrappers.h>
#include <worldresidencyassets.hpp>
#include <pddi/pica/nativereadahead.hpp>
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
#include <worldsim/traffic/trafficmanager.h>
#include <worldsim/ped/pedestrianmanager.h>
#include <events/eventmanager.h>
#include <poser/poseengine.hpp>
#include <worldsim/character/character.h>
#include <worldsim/character/charactermanager.h>
#include <worldsim/redbrick/vehicle.h>
#include <mission/gameplaymanager.h>
#include <mission/missionmanager.h>
#include <mission/mission.h>
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
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
namespace picaCharacterOcclusion { extern unsigned groups,vertices,misses; }
#endif
#include <presentation/gui/guisystem.h>
#include <presentation/gui/ingame/guimanageringame.h>
#include <presentation/gui/ingame/guiscreenmissionload.h>
#include <presentation/gui/ingame/guiscreeniriswipe.h>
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
#endif
extern bool g_picaNativeTextureDiagnostics;
#include <pddi/pica/texture.hpp>
static picaNativeTextureStats nativeTextureCaptureBegin={},nativeTextureCaptureEnd={};
extern "C" void picaDumpNativeTextures(FILE*);
extern "C" void radLoadDumpWorkerSlices(FILE*);
extern "C" void radLoadSetCoreExperiment(int);
#ifdef SRR2_OLD3DS_LEVEL_CACHE
#include <pddi/pica/nativelevelcache.hpp>
#endif
extern "C" void p3dDumpNativeReader(FILE*);
extern bool g_old3dsParkedSectionRemove;
extern "C" void old3dsDumpParkedRemoval(FILE*);
extern "C" unsigned g_old3dsWorldOrderChoice;
extern "C" unsigned g_old3dsWorldResidencyChoice;
extern "C" void srr2DumpWorldOrder(FILE*);
#endif
extern "C" unsigned g_picaPresentedFrames;
extern "C" void picaDumpStereoDiagnostics(FILE*);
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
static unsigned recordCapacity=Capacity;
static unsigned RouteRecordCapacity(unsigned duration,unsigned targetFPS)
{
 // Keep every frame in long road tests. The extra two slots per second cover
 // boundary/pacing jitter; the bounded request cannot grow during gameplay.
 const unsigned required=duration*(targetFPS>30?62u:32u)+64;
 return required>Capacity?required:Capacity;
}
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
 unsigned route, pairs, areas, entities,residencyPolicy;
 float carPosition[3],carVertical,carAngular;unsigned carInPhysics,carRest,physicsLimit;
 float cameraPosition[3],cameraTarget[3]; unsigned activePeds,frustumPeds;
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
static bool residencyCpuProbe=false,missionNpcProbe=false;
static unsigned suspensionProbe=0,lastSuspensionStep=0;
static bool incidentWorldPrepared=false;
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
 rmt::Vector position,normal,ground;GetAvatarManager()->GetAvatarForPlayer(0)->GetPosition(position);
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
static unsigned earlyWorldQueue=2;
static bool stereoClip=false;
static float smallObjectCullSquared=4000.0f;
float SmallObjectCullSquared(){return smallObjectCullSquared;}
// Alternate only the substep limit after placement. No resets or force/pose
// writes occur during the capture; the same simulation evolves continuously.
unsigned SuspensionStep(unsigned accepted){lastSuspensionStep=suspensionProbe==1?(((elapsedMs/10000u)&1u)?25u:50u):accepted;return lastSuspensionStep;}
bool StereoClip(){return stereoClip;}
bool EarlyWorldQueue(){return earlyWorldQueue!=0;}
unsigned EarlyWorldQueuePhase(){return earlyWorldQueue;}
static unsigned trafficLimit=0,trafficVisible=0,trafficFrames=0,trafficActiveTotal=0,trafficVisibleTotal=0,trafficActiveMaximum=0,trafficVisibleMaximum=0;
unsigned AmbientTrafficLimit(unsigned accepted){return trafficLimit?trafficLimit:accepted;}
void CountAmbientTrafficDisplay(){if(collecting)++trafficVisible;}

#ifdef SRR2_OLD3DS_NATIVE_UNIFORMS
static bool nativeUniforms=true;
#else
static bool nativeUniforms=false;
#endif
static unsigned reloadPhase=0,reloadMs=0,reloadDispatches=0,reloadObservedLevels=0;
static bool reloadPauseRequested=false;
static WorldStress::Plan reloadPlan;
static WorldStress::FrameStats* reloadStats=nullptr;
static bool reloadEntered[WorldStress::Plan::Capacity+1]={},reloadSampling=false;
static bool reloadReset[WorldStress::Plan::Capacity+1]={};
static bool reloadTeleported[WorldStress::Plan::Capacity+1]={};
static bool reloadInteriorDone[WorldStress::Plan::Capacity+1]={};
static bool reloadMissionRequested[WorldStress::Plan::Capacity+1]={};
static unsigned reloadReportMs=0;
static WorldStress::PhaseWatchdog reloadPhaseWatchdog;
static bool worldResidencyOverride=false,irisCapture=false;
static unsigned irisClosedFrames=0,irisCaptures=0;
static ZoneEventLocator* reloadTeleportLocators[WorldStress::Plan::Capacity+1]={};
static u64 reloadPeriod=0;

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
static const char* names[Count]={"update","render","stream","service","file_service","debug_service","sound_service","file_lock","file_callback","file_error","file_user_callback","sound_buffer_complete","sound_encode","sound_encode_kernel","load_handoff","load_callback","load_yield","load_lock","load_callback_done","load_callback_release","load_user_callback","load_handler_release","load_next_request","load_zone_event","load_zone_post","load_zone_named","load_zone_pre","load_parked_add","load_parked_reset","load_parked_remove"};
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
bool WorldResidencyOverridden(){return worldResidencyOverride;}
unsigned WorldResidencyChoice(){return __atomic_load_n(&g_old3dsWorldResidencyChoice,__ATOMIC_ACQUIRE);}

void ReloadStereoFrame(){if(reloadSampling && reloadStats && reloadPhase<=reloadPlan.count)++reloadStats[reloadPhase].stereoFrames;}
void ReloadStereoSegment(){if(reloadSampling && reloadStats && reloadPhase<=reloadPlan.count)++reloadStats[reloadPhase].stereoSegments;}
bool IdleLoaderWait(){return WorldStreaming::idleLoader;}
unsigned CurrentRouteMs(){return elapsedMs;}
bool NativeUniforms(){return nativeUniforms;}
// Benchmark-only input driver. Follow immutable road points through the normal
// controller; never move world objects or change simulation/traffic state.
static bool roadRoute=false;
static unsigned roadDirection=0;
static bool fixedRoadAnchor=false;
static rmt::Vector roadAnchor;
static rmt::Vector roadPoints[512],roadLastPosition;
static unsigned roadPointCount=0,roadPoint=0,roadStalledMs=0,roadReverseMs=0,roadRecoveries=0;
static float roadDistance=0,roadPlannedDistance=0;
static unsigned roadBuiltCount=0;
static bool roadPathExhausted=false;
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
 for(unsigned roads=0;roads<128 && roadPointCount<512;++roads){
  ++roadBuiltCount;
  while(index>=0 && index<(int)road->GetNumRoadSegments() && roadPointCount<512){
   segment=road->GetRoadSegment(index);
   segment->GetPosition(forward?1.f:0.f,.5f,&b);
   if(!roadPointCount || (b-roadPoints[roadPointCount-1]).MagnitudeSqr()>4){
    if(roadPointCount)roadPlannedDistance+=std::sqrt((b-roadPoints[roadPointCount-1]).MagnitudeSqr());
    roadPoints[roadPointCount++]=b;
   }
   index+=forward?1:-1;
  }
  if(roadPointCount<2)return false;
  rmt::Vector heading=roadPoints[roadPointCount-1]-roadPoints[roadPointCount-2];
  const float length=std::sqrt(heading.x*heading.x+heading.z*heading.z);
  if(length<.01f)break;
  heading.x/=length;heading.z/=length;
  const Intersection* intersection=forward?road->GetDestinationIntersection():road->GetSourceIntersection();
  if(!intersection)break;
  const Road* next=nullptr;bool nextForward=true;float best=-5;
  for(unsigned group=0;group<2;++group){
   const unsigned count=group?intersection->GetNumRoadsIn():intersection->GetNumRoadsOut();
   for(unsigned i=0;i<count;++i){
    const Road* candidate=group?intersection->GetRoadIn(i):intersection->GetRoadOut(i);
    if(candidate->GetShortCut() || !candidate->GetNumRoadSegments())continue;
    RoadSegment* entry=candidate->GetRoadSegment(group?candidate->GetNumRoadSegments()-1:0);
    entry->GetPosition(group?1.f:0.f,.5f,&a);entry->GetPosition(group?0.f:1.f,.5f,&b);
    const float dx=b.x-a.x,dz=b.z-a.z,n=std::sqrt(dx*dx+dz*dz);
    if(n<.01f)continue;
    // A dead end needs a U-turn. Prefer every ordinary continuation first.
    float alignment=(dx*heading.x+dz*heading.z)/n-(candidate==road?3.0f:0.0f);
    if(WorldStreaming::traversal) {
     const float turn=std::atan2(heading.z*dx-heading.x*dz,heading.x*dx+heading.z*dz);
     const bool left=WorldStreaming::traversal==1 || (WorldStreaming::traversal==3 && ((roads+seed)%2));
     const float desired=roads%3==0?0.f:(left?.95f:-.95f);
     alignment=-std::fabs(turn-desired)-(candidate==road?4.f:0.f);
    }
    if(alignment>best){best=alignment;next=candidate;nextForward=!group;}
   }
  }
  if(!next)break;
  road=next;forward=nextForward;index=forward?0:road->GetNumRoadSegments()-1;
 }
 return roadPointCount>=8 && roadPlannedDistance>=seconds*18.0f+200.0f;
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
 if(roadPoint+1==roadPointCount && target.x*target.x+target.z*target.z<=100){
  roadPathExhausted=true;roadGas=roadSteer=0;roadBrake=1;aborted=done=true;return;
 }
 const float angle=std::atan2(facing.z*target.x-facing.x*target.z,facing.x*target.x+facing.z*target.z);
 float steering=angle/.6f;if(steering>1)steering=1;if(steering<-1)steering=-1;
 roadSteer=int(steering*26000);
 const float speed=vehicle->mSpeed;
 const float desired=std::fabs(angle)>.6f?9.f:WorldStreaming::cruiseSpeed;
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
 if(!strncmp(argument,"bench_landmark_visibility=",26)){Old3dsLandmark::enabled=atoi(argument+26)!=0;return true;}
 if(!strcmp(argument,"bench_landmark=casino")){casinoAnchor=true;return true;}
 static const char distanceOption[]="bench_draw_distance=";
 if(!strncmp(argument,distanceOption,sizeof(distanceOption)-1)){
  const float value=strtof(argument+sizeof(distanceOption)-1,nullptr);
  if(std::isfinite(value) && value>=120 && value<=240)Old3dsPerformance::visibilityDistance=value;
  return true;
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
 else if(!strncmp(argument,"bench_reload_plan=",18)){
  if(!reloadPlan.Parse(argument+18))return false;
  reload=true;
 }
 else if(!strcmp(argument,"bench_iris_capture=1"))irisCapture=true;
 else if(!strcmp(argument,"bench_iris_capture=0"))irisCapture=false;
 else if(!strcmp(argument,"bench_reload_active=1"))reloadPlan.activeMission=true;
 else if(!strcmp(argument,"bench_reload_active=0"))reloadPlan.activeMission=false;
 else if(!strcmp(argument,"bench_reload_interior=1"))reloadPlan.interior=true;
 else if(!strcmp(argument,"bench_reload_interior=0"))reloadPlan.interior=false;
 else if(!strcmp(argument,"bench_reload_teleport=1"))reloadPlan.teleport=true;
 else if(!strcmp(argument,"bench_reload_teleport=0"))reloadPlan.teleport=false;
 else if(!strcmp(argument,"bench_reload_reset=1"))reloadPlan.resetMission=true;
 else if(!strcmp(argument,"bench_reload_reset=0"))reloadPlan.resetMission=false;
 else if(!strncmp(argument,"bench_reload_dwell=",19)){
  char* end=nullptr;const char* value=argument+19;const unsigned long seconds=strtoul(value,&end,10);
  if(end==value || *end || seconds<3 || seconds>30)return false;
  reloadPlan.dwellMs=unsigned(seconds)*1000;
 }

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
 else if(!strncmp(argument,"bench_small_distance=",21)){
  const char* value=argument+21;char* end=nullptr;const float distance=strtof(value,&end);
  if(value==end || *end || !std::isfinite(distance) || distance<64 || distance>240)return false;
  smallObjectCullSquared=distance*distance;
 }
 else if(!strcmp(argument,"bench_mission_npc_probe=1"))missionNpcProbe=true;
 else if(!strcmp(argument,"bench_mission_npc_probe=0"))missionNpcProbe=false;
 else if(!strcmp(argument,"bench_suspension_probe=2"))suspensionProbe=2;
 else if(!strcmp(argument,"bench_suspension_probe=1"))suspensionProbe=1;
 else if(!strcmp(argument,"bench_suspension_probe=0"))suspensionProbe=0;
 else if(!strcmp(argument,"bench_parked_section_remove=1"))g_old3dsParkedSectionRemove=true;
 else if(!strcmp(argument,"bench_parked_section_remove=0"))g_old3dsParkedSectionRemove=false;
 else if(!strcmp(argument,"bench_residency_cpu_probe=1"))residencyCpuProbe=true;
 else if(!strcmp(argument,"bench_residency_cpu_probe=0"))residencyCpuProbe=false;
 else if(!strcmp(argument,"bench_resident_lookup_cache=0"))WorldResidency::CacheLookup(false);
 else if(!strcmp(argument,"bench_resident_lookup_cache=1"))WorldResidency::CacheLookup(true);
 else if(!strncmp(argument,"bench_road_anchor=",18)){
  rmt::Vector anchor;char trailing=0;
  if(sscanf(argument+18,"%f,%f,%f%c",&anchor.x,&anchor.y,&anchor.z,&trailing)!=3 ||
     !std::isfinite(anchor.x) || !std::isfinite(anchor.y) || !std::isfinite(anchor.z) ||
     std::fabs(anchor.x)>10000 || std::fabs(anchor.y)>10000 || std::fabs(anchor.z)>10000)return false;
  roadAnchor=anchor;fixedRoadAnchor=true;
 }
 else if(!strncmp(argument,"bench_road_direction=",21)){
  const char* value=argument+21;
  if(!strcmp(value,"forward"))roadDirection=1;
  else if(!strcmp(value,"reverse"))roadDirection=2;
  else if(!strcmp(value,"west"))roadDirection=3;
  else return false;
 }
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
 else if(!strncmp(argument,"bench_vram_compact=",19))g_picaVramCompact=atoi(argument+19)!=0;
#ifdef SRR2_OLD3DS_NATIVE_ETC1
 else if(!strcmp(argument,"bench_linear_compact=0"))g_picaLinearCompact=false;
 else if(!strcmp(argument,"bench_linear_compact=1"))g_picaLinearCompact=true;
#endif

#endif
 else if(!strncmp(argument,"bench_traffic_limit=",20)){
  const char* value=argument+20;char* end=nullptr;unsigned long limit=strtoul(value,&end,10);
  if(value==end || *end || limit<1 || limit>5)return false;
  trafficLimit=unsigned(limit);
 }
#if defined(SRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE) && defined(SRR2_OLD3DS_NATIVE_ETC1)
 else if(!strncmp(argument,"bench_exact_world_cache=",24))g_picaNativeExactWorldFallback=atoi(argument+24)!=0;
 else if(!strncmp(argument,"bench_native_unbuffered=",24))g_picaNativeBundleUnbuffered=atoi(argument+24)!=0;
 else if(!strcmp(argument,"bench_native_coalesced=1"))g_picaNativeBundleCoalesced=true;
 else if(!strcmp(argument,"bench_native_coalesced=0"))g_picaNativeBundleCoalesced=false;
 else if(!strcmp(argument,"bench_native_async=1"))picaNativeRead::enabled=true;
 else if(!strcmp(argument,"bench_native_async=0"))picaNativeRead::enabled=false;
#endif
#ifdef SRR2_OLD3DS_LEVEL_CACHE
 else if(!strcmp(argument,"bench_native_cache=0"))return picaNativeLevel::SetMode(0);
 else if(!strcmp(argument,"bench_native_cache=1"))return picaNativeLevel::SetMode(1);
 else if(!strcmp(argument,"bench_native_cache=2"))return picaNativeLevel::SetMode(2);
#endif
 else if(!strcmp(argument,"bench_loader_core=0"))radLoadSetCoreExperiment(0);
 else if(!strcmp(argument,"bench_loader_core=1")) {
#ifdef SRR2_OLD3DS_LEVEL_CACHE
  if(picaNativeLevel::Mode())return false;
#endif
  radLoadSetCoreExperiment(1);
 }
 else if(!strcmp(argument,"bench_world_residency=0")){worldResidencyOverride=true;__atomic_store_n(&g_old3dsWorldResidencyChoice,0u,__ATOMIC_RELEASE);}
 else if(!strcmp(argument,"bench_world_residency=1")){worldResidencyOverride=true;__atomic_store_n(&g_old3dsWorldResidencyChoice,1u,__ATOMIC_RELEASE);}
 else if(!strcmp(argument,"bench_world_residency=2")){worldResidencyOverride=true;__atomic_store_n(&g_old3dsWorldResidencyChoice,2u,__ATOMIC_RELEASE);}
 else if(!strcmp(argument,"bench_world_residency=3")){worldResidencyOverride=true;__atomic_store_n(&g_old3dsWorldResidencyChoice,3u,__ATOMIC_RELEASE);}
 else if(!strcmp(argument,"bench_world_residency=4")){worldResidencyOverride=true;__atomic_store_n(&g_old3dsWorldResidencyChoice,4u,__ATOMIC_RELEASE);}
 else if(!strcmp(argument,"bench_world_order=0"))__atomic_store_n(&g_old3dsWorldOrderChoice,0u,__ATOMIC_RELEASE);
 else if(!strcmp(argument,"bench_world_order=1"))__atomic_store_n(&g_old3dsWorldOrderChoice,1u,__ATOMIC_RELEASE);
 else if(!strcmp(argument,"bench_world_order=2"))__atomic_store_n(&g_old3dsWorldOrderChoice,2u,__ATOMIC_RELEASE);
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
 else if(!strncmp(argument,"bench_fog_reuse=",16))picaNativeEnableFogReuse(atoi(argument+16)!=0);
#endif
 else if(!strncmp(argument,"bench_early_world_queue=",24)){
  const char* value=argument+24;char* end=nullptr;unsigned long phase=strtoul(value,&end,10);
  if(value==end || *end || phase>2)return false;
  earlyWorldQueue=unsigned(phase);
 }
 else if(!strncmp(argument,"bench_stereo_clip=",18))stereoClip=atoi(argument+18)!=0;
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
#ifdef SRR2_OLD3DS_NATIVE_TEXTURE_BUNDLE
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
struct PassiveFrame { unsigned ms,context,period,work,linear,queued;float x,y,z,speed; };
static constexpr unsigned PassiveCapacity=2048;
static PassiveFrame* passiveFrames=nullptr;
static unsigned passiveCount=0,passiveMs=0,passiveContext=0,passiveLastGameplay=0;
static u64 passiveStart=0,passivePrevious=0;
static bool passiveMarked=false;
static void PassiveBegin(int context)
{
    const u64 now=svcGetSystemTick();
    passiveMs=unsigned((now-passiveStart)*1000/SYSCLOCK_ARM11);elapsedMs=passiveMs;
    passiveContext=context;WorldStreaming::passiveActive=context==CONTEXT_GAMEPLAY;
    WorldStreaming::BeginFrame(passiveMs,0.f);
}
static bool PassiveEnd(u64 work,int context)
{
    const u64 now=svcGetSystemTick();
    if(WorldStreaming::passiveActive)WorldStreaming::EndFrame();
    if(passiveFrames && WorldStreaming::passiveActive) {
        passiveLastGameplay=passiveMs;
        PassiveFrame& r=passiveFrames[passiveCount++%PassiveCapacity];r={};
        r.ms=passiveMs;r.context=context;r.period=unsigned(now-passivePrevious);r.work=unsigned(work);
        r.linear=linearSpaceFree();r.queued=GetLoadingManager()->GetNumCurrentRequests();
        if(WorldStreaming::passiveActive) {
            Avatar* a=GetAvatarManager()->GetAvatarForPlayer(0);rmt::Vector p;a->GetPosition(p);
            r.x=p.x;r.y=p.y;r.z=p.z;r.speed=a->GetSpeedMps();
        }
    }
    passivePrevious=now;
    const u32 marker=KEY_START|KEY_SELECT;
    if((hidKeysHeld()&marker)==marker) {
        passiveMarked=true;WorldStreaming::MarkerImage();return true;
    }
    return false;
}
static void DumpPassive()
{
    WorldStreaming::passiveActive=false;
    FILE* f=fopen("native-" SRR2_OLD3DS_BUILD_ID ".log","w");if(!f)return;
    fprintf(f,"NATIVE_RUN id=%s passive=1\n",runIdentity);
    fprintf(f,"PASSIVE_CONFIG no_automatic_input=1 no_placement=1 no_fixed_seed=1 no_fixed_step=1 marker=%u window_ms=20000 performance_comparison=0\n",passiveMarked);
    Avatar* recordedAvatar=GetAvatarManager()->GetAvatarForPlayer(0);
    Vehicle* recordedCar=recordedAvatar && recordedAvatar->IsInCar()?recordedAvatar->GetVehicle():nullptr;
    fprintf(f,"PASSIVE_VEHICLE incar=%u model=%s\n",recordedCar!=nullptr,recordedCar?recordedCar->GetName():"none");
    WorldResidency::Dump(f);GetRenderManager()->DumpZoneQueue(f);WorldStreaming::Dump(f,runIdentity);
    picaDumpStereoDiagnostics(f);
#ifdef SRR2_OLD3DS_NATIVE_TEXTURES
    picaDumpNativeTextures(f);
    radLoadDumpWorkerSlices(f);
#ifdef SRR2_OLD3DS_LEVEL_CACHE
 picaNativeLevel::Finish();picaNativeLevel::Dump(f);
#endif
    p3dDumpNativeReader(f);
 old3dsDumpParkedRemoval(f);
    srr2DumpWorldOrder(f);
#endif
    fprintf(f,"PASSIVE_WINDOW reference_ms=%u marker_ms=%u pause_gap_ms=%u marker_context=%u gameplay_frames_only=1\n",passiveLastGameplay,passiveMs,passiveMs-passiveLastGameplay,passiveContext);
    for(unsigned i=passiveCount>PassiveCapacity?passiveCount-PassiveCapacity:0;i<passiveCount;++i) {
        const PassiveFrame& r=passiveFrames[i%PassiveCapacity];if(passiveLastGameplay-r.ms>20000)continue;
        fprintf(f,"PASSIVE_FRAME ms=%u context=%u period_ms=%.4f work_ms=%.4f linear=%u queued=%u x=%.5f y=%.5f z=%.5f speed=%.5f\n",r.ms,r.context,r.period*1000.0/SYSCLOCK_ARM11,r.work*1000.0/SYSCLOCK_ARM11,r.linear,r.queued,r.x,r.y,r.z,r.speed);
    }
    for(unsigned slot=0;slot<4;++slot)fprintf(f,"NATIVE_SAVE_FIXTURE id=%08x slot=%u bytes=%u fnv1a=%08x personal_save_writes=0\n",saveFixture,slot+1,fixtureBytes[slot],fixtureHash[slot]);
    fprintf(f,"PASSIVE_CAPTURE_END marked=%u time_ms=%u frames=%u\n",passiveMarked,passiveMs,passiveCount);fclose(f);
}
void Init(int argc,char** argv){
 captureTls=getThreadLocalStorage();
 initializationTick=svcGetSystemTick();
#if defined(SRR2_OLD3DS_VEHICLE_PROFILE) || defined(SRR2_OLD3DS_NATIVE_BENCH)
 Old3dsVehicleProfile::CaptureEnabled()=false;
#endif
 if(!PrepareSaveFixtures()){printf("NATIVE_AUTOMATION save_fixture_failed abort=1\n");aborted=done=true;return;}
 for(int i=1;i<argc;++i)if(!strcmp(argv[i],"bench_passive=1"))WorldStreaming::passive=true;
 if(WorldStreaming::passive) {
    enabled=false;serviceProfile=true;WorldStreaming::audit=true;WorldStreaming::predictive=false;
    for(int i=1;i<argc;++i) {
        if(!strcmp(argv[i],"bench_idle_loader=0"))WorldStreaming::idleLoader=false;
        if(!strcmp(argv[i],"bench_idle_loader=1"))WorldStreaming::idleLoader=true;
    }
    for(int i=1;i<argc;++i)if(!strncmp(argv[i],"bench_trial=",12) ||
        !strncmp(argv[i],"bench_world_residency=",22) ||
        !strncmp(argv[i],"bench_loader_core=",18) ||
        !strncmp(argv[i],"bench_vram_compact=",19) ||
        !strncmp(argv[i],"bench_linear_compact=",21) ||
        !strncmp(argv[i],"bench_native_cache=",19))ParseRunArgument(argv[i]);
    WorldResidency::Init();WorldStreaming::Init();passiveFrames=static_cast<PassiveFrame*>(calloc(PassiveCapacity,sizeof(PassiveFrame)));
    passiveStart=passivePrevious=svcGetSystemTick();
    printf("PASSIVE_LAUNCH trial=%s marker=START_SELECT original_saves_protected=1\n",runIdentity);
    return;
 }

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
  if(WorldStreaming::ParseArgument(argv[i]))continue;
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
#endif
  if(!strncmp(argv[i],"bench_route=",12)){roadRoute=!strcmp(argv[i]+12,"road");continue;}
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
  static const char capturePromoteArgument[]="bench_vram_capture_promote=",copyDiagnosticsArgument[]="bench_vram_copy_diagnostics=";
  if(!strncmp(argv[i],capturePromoteArgument,sizeof(capturePromoteArgument)-1)){g_picaVramCapturePromote=atoi(argv[i]+sizeof(capturePromoteArgument)-1)!=0;continue;}
  if(!strncmp(argv[i],copyDiagnosticsArgument,sizeof(copyDiagnosticsArgument)-1)){g_picaVramCopyDiagnostics=atoi(argv[i]+sizeof(copyDiagnosticsArgument)-1)!=0;continue;}
#endif
  static const char loaderSliceArgument[]="bench_loader_slice_us=";
  if(!strncmp(argv[i],loaderSliceArgument,sizeof(loaderSliceArgument)-1)){unsigned valueUs=strtoul(argv[i]+sizeof(loaderSliceArgument)-1,nullptr,10);loaderSliceUs=valueUs>=1000 && valueUs<=8000?valueUs:0;continue;}

  static const char preparedFontArgument[]="bench_prepared_font_glyphs=";
  if(!strncmp(argv[i],preparedFontArgument,sizeof(preparedFontArgument)-1)){p3dFontWork::enabled=atoi(argv[i]+sizeof(preparedFontArgument)-1)!=0;continue;}
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
 WorldResidency::Init();WorldStreaming::Init();
 const unsigned minimumSeconds=portrait?1u:5u,minimumWarmup=portrait?0u:3u;
 if(seconds<minimumSeconds||seconds>(roadRoute?120u:25u))seconds=25;
 if(warmup<minimumWarmup||warmup>60)warmup=8;if(step<1||step>100)step=67;
 // Use the existing normal heap after libctru startup, not additional BSS.
 recordCapacity=RouteRecordCapacity(seconds,Graphics3DS::TargetFPS());
 records=static_cast<Record*>(calloc(recordCapacity,sizeof(Record)));
 if(reload){
  reloadStats=static_cast<WorldStress::FrameStats*>(malloc((reloadPlan.count+1)*sizeof(WorldStress::FrameStats)));
  if(!reloadStats){enabled=false;aborted=done=true;printf("NATIVE_AUTOMATION reload_stats_allocation_failed abort=1\n");return;}
  for(unsigned i=0;i<=reloadPlan.count;++i)::new(static_cast<void*>(reloadStats+i)) WorldStress::FrameStats();
 }

 printf("NATIVE_LAUNCH id=%s mode=%s seconds=%u checkpoint_io=%u\n",runIdentity,replay?"replay":"realtime",seconds,checkpointIO);
 if(!records){enabled=false;aborted=true;done=true;
  printf("NATIVE_AUTOMATION allocation_failed bytes=%u abort=1\n",unsigned(recordCapacity*sizeof(Record)));return;}
 bool savedDetail=detail;detail=true;collecting=true;u64 cal=svcGetSystemTick();for(unsigned i=0;i<4096;++i){Scope test(Update);}scopeCalibration=svcGetSystemTick()-cal;collecting=false;detail=savedDetail;
 char a[]="skipfe",b[]="notutorial",c[]="L2";
 if(portraitLevel>=1 && portraitLevel<=7)c[1]='0'+portraitLevel;
 CommandLineOptions::HandleOption(a);CommandLineOptions::HandleOption(b);CommandLineOptions::HandleOption(c);
}
unsigned Seed(unsigned fallback){return enabled?seed:fallback;}
static bool PrepareIncidentWorld()
{
 // The reported revisit had Z2 resident before its unload. Recreate that known
 // initial state before warmup; measured traversal never waits for resources.
 TriggerVolumeTracker* tracker=GetTriggerVolumeTracker();
 for(unsigned i=0;i<tracker->GetNumTriggers();++i){
  Locator* locator=tracker->GetTrigger(i)->GetLocator();
  if(!locator || locator->GetDataType()!=LocatorType::DYNAMIC_ZONE)continue;
  auto* zone=static_cast<ZoneEventLocator*>(locator);
  if(zone->IsInteriorLoad())continue;
  unsigned mask=0;
  for(int n=0;n<zone->GetNumLoadZones();++n){const char* name=zone->GetLoadZone(n);
   if(!strcmp(name,"l2z2.p3d"))mask|=1;
   if(!strcmp(name,"l2z1.p3d"))mask|=2;
   if(!strcmp(name,"l2r1.p3d"))mask|=4;
   if(!strcmp(name,"l2r4.p3d"))mask|=8;
  }
  if(mask==15){zone->Trigger(0,true);return true;}
 }
 return false;
}
static void UpdateStreamingFlight(Avatar*,unsigned);
// Diagnostic warp follows the engine's debug-teleport protocol: request a
// real authored zone, then reset the physical vehicle/character on a real road.
// Owned locator copies outlive queued callbacks and level heap changes.
static bool ReloadTeleport(unsigned phase)
{
 Avatar* avatar=GetAvatarManager()->GetAvatarForPlayer(0);if(!avatar || !avatar->GetCharacter())return false;
 rmt::Vector from;avatar->GetPosition(from);ZoneEventLocator* selected=nullptr;
 rmt::Vector target,facing;float farthest=150.f*150.f;
 TriggerVolumeTracker* tracker=GetTriggerVolumeTracker();const unsigned map=unsigned(GetGameplayManager()->GetCurrentLevelIndex())+1;
 for(unsigned i=0;i<tracker->GetNumTriggers();++i){
  TriggerVolume* volume=tracker->GetTrigger(i);Locator* locator=volume->GetLocator();
  if(!locator || locator->GetDataType()!=LocatorType::DYNAMIC_ZONE)continue;
  auto* zone=static_cast<ZoneEventLocator*>(locator);if(zone->IsInteriorLoad() || zone->IsInteriorDump() || !zone->GetNumLoadZones())continue;
  bool exterior=true;
  for(int n=0;n<zone->GetNumLoadZones();++n)exterior=exterior && WorldResidencyAssets::Sector(map,WorldResidencyAssets::LowerUID(zone->GetLoadZone(n)));
  if(!exterior)continue;
  RoadSegment* road=nullptr;float roadDistance=0;GetIntersectManager()->FindClosestRoad(volume->GetPosition(),60.f,road,roadDistance);if(!road)continue;
  rmt::Vector position,a,b;road->GetPosition(.5f,.5f,&position);const float distance=(position-from).MagnitudeSqr();if(distance<=farthest)continue;
  rmt::Vector probe=position,normal,ground;bool hit=false;probe.y+=3.f;GetIntersectManager()->FindIntersection(probe,hit,normal,ground);if(!hit)continue;
  road->GetPosition(0,.5f,&a);road->GetPosition(1,.5f,&b);selected=zone;target=position;facing=b-a;farthest=distance;
 }
 char zoneText[254];if(!WorldStress::ExteriorZoneText(selected,zoneText))return false;
 auto* owned=new(GMA_PERSISTENT) ZoneEventLocator();if(!owned)return false;owned->AddRef();
 owned->SetZoneSize(unsigned(std::strlen(zoneText))+1);owned->SetZone(zoneText);owned->SetPlayerEntered();reloadTeleportLocators[phase]=owned;
 if(owned->GetNumLoadZones()!=selected->GetNumLoadZones() || owned->GetNumDumpZones()!=selected->GetNumDumpZones()
    || owned->GetNumLWSActivates()!=selected->GetNumLWSActivates() || owned->GetNumLWSDeactivates()!=selected->GetNumLWSDeactivates())return false;
 GetRenderManager()->mpLayer(RenderEnums::LevelSlot)->DumpAllDynaLoads(1,GetRenderManager()->mEntityDeletionList);
 GetEventManager()->TriggerEvent(EVENT_FIRST_DYNAMIC_ZONE_START,owned);
 Character* character=avatar->GetCharacter();
 if(character->IsInCar() && character->GetTargetVehicle()){
  Vehicle* vehicle=character->GetTargetVehicle();vehicle->SetInitialPositionGroundOffsetAutoAdjust(&target);
  vehicle->mResetFacingRadians=std::atan2(facing.x,facing.z);vehicle->Reset(true,false);
 }else character->RelocateAndReset(target,std::atan2(facing.x,facing.z));
 GetSuperCamManager()->GetSCC(0)->NoTransition();GetSuperCamManager()->GetSCC(0)->DoCameraCut();
 printf("NATIVE_RELOAD_ACTION phase=%u action=teleport level=%u distance=%.3f x=%.5f y=%.5f z=%.5f\n",phase,map,std::sqrt(farthest),target.x,target.y,target.z);
 return true;
}

// Exercise the production mission-actor model lifetime without advancing or
// fabricating mission stages. Three different actors cover the script's maximum
// simultaneous NPC count. Level 2 uses the authored bm2_svt blood-mission
// locator; other maps use an independent distant point on their road route.
// All teleports precede arrival checks.
static Character* missionProbeActors[3]={};
static const char* missionProbeModels[3]={"teen","moe","grandpa"};
static bool missionProbeStarted=false,missionProbeArrived=false,missionProbeRemoved=false;
static unsigned missionProbeFarReady=0;
static const char* missionProbeFailure="none";
static unsigned missionProbeGroundRejects=0;
static rmt::Vector missionProbeOrigin(45.79897f,6.876108f,-52.38862f);
static void DumpVisualEye(FILE*,gfx3dSide_t,const char*,gfxScreen_t,bool);
static bool MissionNPCProbeStep(unsigned ms)
{
    auto* manager=GetCharacterManager();auto* avatar=GetAvatarManager()->GetAvatarForPlayer(0);
    Character* player=avatar?avatar->GetCharacter():nullptr;
    auto fail=[](const char* reason){missionProbeFailure=reason;return false;};
    if(!player || portraitLevel<1 || portraitLevel>7)return fail("invalid_player_or_level");
    if(!missionProbeStarted){
        if(player->IsInCar())GetAvatarManager()->PutCharacterOnGround(player,GetGameplayManager()->GetCurrentVehicle());
        rmt::Vector farPosition(163.32f,10.548828f,308.04f);
        if(portraitLevel!=2){
            if(!roadPointCount)return fail("missing_road_route");
            farPosition=roadPoints[0];float farthest=175.f*175.f;bool selected=false;
            // Route endpoints can lie over bridges or outside a loaded collision
            // patch. Choose the farthest point with real ground rather than
            // treating one unsuitable endpoint as a game failure.
            for(unsigned i=1;i<roadPointCount;++i){
                const float distance=(roadPoints[i]-farPosition).MagnitudeSqr();
                if(distance<=farthest)continue;
                rmt::Vector probe=roadPoints[i],normal,ground;probe.y+=2.f;bool found=false;
                GetIntersectManager()->FindIntersection(probe,found,normal,ground);
                if(!found){++missionProbeGroundRejects;continue;}
                farthest=distance;missionProbeOrigin=roadPoints[i];missionProbeOrigin.y=ground.y;selected=true;
            }
            if(!selected)return fail("no_distant_ground");
        }
        player->RelocateAndReset(farPosition,0.f);
        avatar->SetCameraTargetToCharacter(true);
        for(unsigned i=0;i<3;++i){
            char name[32];std::snprintf(name,sizeof(name),"resident_mission_probe_%u",i);
            Character* actor=manager->AddCharacter(CharacterManager::NPC,name,missionProbeModels[i],"npd",nullptr);
            if(!actor)return fail("actor_creation_failed");
            actor->AddRef();missionProbeActors[i]=actor;actor->SetRole(Character::ROLE_MISSION);
            rmt::Vector position=missionProbeOrigin;position.x+=2.5f*i;
            actor->RelocateAndReset(position,0.f);actor->AddToWorldScene();manager->SetGarbage(actor,false);
        }
        missionProbeStarted=true;
        float minimumDistance=1.e30f;rmt::Vector actualPlayer;player->GetPosition(actualPlayer);
        for(auto* actor:missionProbeActors){rmt::Vector position;actor->GetPosition(position);const float distance=(position-actualPlayer).Magnitude();if(distance<minimumDistance)minimumDistance=distance;}
        std::printf("NATIVE_MISSION_NPC_PROBE phase=spawn level=%u actors=3 player_distance_min=%.3f active_role=MISSION performance_capture=0\n",portraitLevel,minimumDistance);
        if(minimumDistance<=150.f)return fail("insufficient_actor_distance");
        Old3dsMemoryDump(stdout);
    }
    if(ms>=6000 && !missionProbeFarReady){
        for(unsigned i=0;i<3;++i){
            if(!manager->IsModelLoaded(missionProbeModels[i]) || !missionProbeActors[i]->NativeBenchModelReady()){
                std::printf("NATIVE_MISSION_NPC_PROBE phase=far_ready failed_model=%s ready=0 abort=1\n",missionProbeModels[i]);return fail("far_model_not_ready");
            }
        }
        missionProbeFarReady=3;
        std::printf("NATIVE_MISSION_NPC_PROBE phase=far_ready ready=3 actors=3\n");Old3dsMemoryDump(stdout);
    }
    if(ms>=8000 && !missionProbeArrived){
        rmt::Vector position=missionProbeOrigin;position.z+=8.f;player->RelocateAndReset(position,0.f);
        SuperCamCentral* central=GetSuperCamManager()->GetSCC(0);
        StaticCam* camera=static_cast<StaticCam*>(central->GetSuperCam(SuperCam::STATIC_CAM));
        if(!camera){camera=new StaticCam();central->RegisterSuperCam(camera);}
        camera->SetTarget(player->GetTarget());camera->SetTracking(false);
        position=missionProbeOrigin;position.x+=2.5f;position.y+=1.1f;camera->SetTargetOffset(position);
        position.z+=6.f;position.y+=.5f;camera->SetPosition(position);camera->SetMaxFOV(1.35f);camera->SetFOV(1.35f);
        central->SelectSuperCam(camera,SuperCamCentral::CUT|SuperCamCentral::FORCE);
        for(unsigned i=0;i<3;++i)if(!missionProbeActors[i]->NativeBenchModelReady())return fail("arrival_model_not_ready");
        missionProbeArrived=true;
        std::printf("NATIVE_MISSION_NPC_PROBE phase=arrival ready=3 actors=3 invisible_placeholders=0\n");
    }
    if(ms>=12000 && !missionProbeRemoved){
        for(unsigned i=0;i<3;++i)if(!missionProbeActors[i]->NativeBenchModelReady())return fail("visible_model_not_ready");
        FILE* image=fopen("sdmc:/3ds/hitrun-dev/mission-npc-probe-frame.log","w");
        if(image){C3D_FrameSync();DumpVisualEye(image,GFX_LEFT,"mission-npc-visible",GFX_TOP,false);fclose(image);}
        for(auto*& actor:missionProbeActors){manager->RemoveCharacter(actor);actor->Release();actor=nullptr;}
        missionProbeRemoved=true;
        std::printf("NATIVE_MISSION_NPC_PROBE phase=remove actors=0\n");Old3dsMemoryDump(stdout);
    }
    if(ms>=17000 && missionProbeRemoved){
        for(unsigned i=0;i<3;++i){char name[32];std::snprintf(name,sizeof(name),"resident_mission_probe_%u",i);if(manager->GetCharacterByName(name))return fail("actor_cleanup_failed");}
    }
    return true;
}

unsigned Begin(unsigned elapsed,int context){
 if(context==CONTEXT_GAMEPLAY)WorldResidency::Gameplay(unsigned(GetGameplayManager()->GetCurrentLevelIndex())+1);
#ifdef SRR2_OLD3DS_LEVEL_CACHE
 if(context==CONTEXT_GAMEPLAY && picaNativeLevel::Mode())
    picaNativeLevel::Start(unsigned(GetGameplayManager()->GetCurrentLevelIndex())+1);
 picaNativeLevel::Tick();
#endif
 if(WorldStreaming::passive){PassiveBegin(context);return elapsed;}
 trafficVisible=0;
 u64 now=svcGetSystemTick();
 reloadPeriod=lastBeginTick?now-lastBeginTick:0;reloadSampling=false;
 if(previousCaptured){unsigned ticks=(unsigned)(now-lastBeginTick);records[(recordCount-1)%recordCapacity].period=ticks;periodSum+=ticks;++periodSamples;previousCaptured=false;}
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
  if(reload && now-benchmarkStart>static_cast<u64>(reloadPlan.WatchdogSeconds())*SYSCLOCK_ARM11){aborted=true;reloadPhase=reloadPlan.count+1;return elapsed;}
  if(reload && reloadPhase<=reloadPlan.count && reloadPhaseWatchdog.Expired(reloadPhase,now,SYSCLOCK_ARM11)){
   printf("NATIVE_AUTOMATION reload_phase_timeout phase=%u seconds=%u abort=1\n",reloadPhase,WorldStress::PhaseWatchdog::Seconds);
   aborted=true;reloadPhase=reloadPlan.count+1;return elapsed;
  }
  if(reload && reloadPhase>0 && reloadPhase<=reloadPlan.count && reloadPlan.interior && reloadEntered[reloadPhase] && !reloadInteriorDone[reloadPhase]){
   const int progress=WorldInteriorStress::Step(reloadPhase,elapsed,context);reloadMs=0;
   if(progress<0){aborted=true;reloadPhase=reloadPlan.count+1;}
   else if(progress>0){reloadInteriorDone[reloadPhase]=true;++reloadStats[reloadPhase].interiors;}
   return elapsed;
  }
  if(reload && reloadPhase<reloadPlan.count+1){
   CGuiManagerInGame* manager=GetGuiSystem()->GetInGameManager();
   auto* gameplay=GetGameplayManager();auto* mission=gameplay?gameplay->GetCurrentMission():nullptr;
   reloadReportMs+=elapsed;
   if(reloadReportMs>=30000){reloadReportMs=0;printf("NATIVE_RELOAD_WAIT phase=%u context=%d loading=%u loading_state=%u gui=%d name=%s sunday_drive=%u linear=%u\n",reloadPhase,context,GetLoadingManager()->IsLoading(),gameplay && mission?unsigned(GetMissionManager()->GetLoadingState()):~0u,manager?int(manager->GetCurrentScreen()):-1,mission?mission->GetName():"none",gameplay && mission?gameplay->IsSundayDrive():true,linearSpaceFree());}
   if(manager && gameplay && mission && reloadPlan.activeMission && manager->IsLoadingNewMission() && manager->GetCurrentWindow() && manager->GetCurrentWindow()->IsRunning()
      && !GetLoadingManager()->IsLoading() && GetMissionManager()->GetLoadingState()==MissionManager::STATE_INVALID){
    auto* screen=static_cast<CGuiScreenMissionLoad*>(manager->GetCurrentWindow());
    if(screen->IsLoadDone())screen->HandleMessage(GUI_MSG_CONTROLLER_SELECT);
   }
   // The engine state and initialized stage establish mission readiness.
   // Background streaming still advances gameplay and its mission timers; it
   // must also advance this dwell clock and appear in the frame statistics.
   // In-game GUI ownership disappears during normal level teardown.
   if(manager && gameplay && mission && context==CONTEXT_GAMEPLAY
      && GetMissionManager()->GetLoadingState()==MissionManager::STATE_INVALID && !GetMissionManager()->InResetState()
      && mission->GetNumStages()>0 && mission->GetCurrentStage()){
    const unsigned expected=reloadPhase?reloadPlan.levels[reloadPhase-1]:(portraitLevel?portraitLevel-1:1);
    const bool missionMatches=!reloadPhase || unsigned(GetGameplayManager()->GetCurrentMissionIndex())==reloadPlan.missions[reloadPhase-1];
    if(unsigned(GetGameplayManager()->GetCurrentLevelIndex())==expected && missionMatches){
     if(reloadPhase && reloadPlan.activeMission && GetGameplayManager()->IsSundayDrive()){
      if(!reloadMissionRequested[reloadPhase]){reloadMissionRequested[reloadPhase]=true;reloadMs=0;GetGameplayManager()->NextMission();printf("NATIVE_RELOAD_ACTION phase=%u action=start_mission\n",reloadPhase);}
      return elapsed;
     }
     reloadObservedLevels|=1u<<expected;reloadMs+=elapsed;
     if(!reloadEntered[reloadPhase]){
      GetAllWrappers()->DumpIntegrity(stdout);Old3dsMemoryDump(stdout);
      reloadEntered[reloadPhase]=true;
      if(reloadPhase){const auto status=WorldResidency::Status(expected+1);auto& stats=reloadStats[reloadPhase];
       stats.missionActive=!GetGameplayManager()->IsSundayDrive();stats.residentExpected=status.expected;stats.residentBound=status.bound;stats.residentBad=status.bad;stats.residentWanted=status.wanted;}
      printf("NATIVE_RELOAD_ENTER phase=%u level=%u mission=%u mission_index=%u name=%s stages=%d\n",reloadPhase,expected+1,unsigned(GetGameplayManager()->GetCurrentMissionIndex())+1-(expected==0?1:0),unsigned(GetGameplayManager()->GetCurrentMissionIndex()),GetGameplayManager()->GetCurrentMission()->GetName(),GetGameplayManager()->GetCurrentMission()->GetNumStages());
     }else reloadSampling=reloadPhase!=0;
     if(reloadPhase && reloadPlan.interior && !reloadInteriorDone[reloadPhase])return elapsed;
     if(reloadPhase && reloadPlan.teleport && !reloadTeleported[reloadPhase]){
      reloadTeleported[reloadPhase]=true;reloadSampling=false;reloadMs=0;
      if(!ReloadTeleport(reloadPhase)){printf("NATIVE_RELOAD_ACTION phase=%u action=teleport_failed\n",reloadPhase);aborted=true;reloadPhase=reloadPlan.count+1;}
      else ++reloadStats[reloadPhase].teleports;
      return elapsed;
     }
     if(reloadPhase && reloadPlan.resetMission && !reloadReset[reloadPhase] && reloadMs>=reloadPlan.dwellMs/2){
      reloadReset[reloadPhase]=true;reloadSampling=false;reloadMs=0;++reloadStats[reloadPhase].resets;
      // Use the normal restart path, including dynamic-zone teardown and the
      // mission's own reset/teleport logic; do not force invalid stage order.
      GetGameplayManager()->RestartCurrentMission();
      printf("NATIVE_RELOAD_ACTION phase=%u action=restart\n",reloadPhase);
      return elapsed;
     }
     if(reloadPhase==0 || reloadMs>=reloadPlan.dwellMs){
      reloadSampling=false;
      if(reloadPhase==reloadPlan.count)reloadPhase=reloadPlan.count+1;
      else if(!reloadPauseRequested){
       // Use the same PauseContext teardown and Mission Select publication
       // path as a player, rather than directly changing level ownership.
       manager->HandleMessage(GUI_MSG_PAUSE_INGAME);reloadPauseRequested=true;
      }
     }
    }
   }
   if(manager && reloadPauseRequested && context==CONTEXT_PAUSE && manager->GetCurrentWindow()
       && manager->GetCurrentWindow()->IsRunning()){
    manager->HandleMessage(GUI_MSG_QUIT_INGAME_FOR_RELOAD,reloadPlan.levels[reloadPhase],reloadPlan.missions[reloadPhase]);
    ++reloadPhase;++reloadDispatches;reloadMs=0;reloadPauseRequested=false;
   }
  }

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
 if(WorldStreaming::incidentRoute==3 && !incidentWorldPrepared){
  if(!WorldStreaming::fly || portraitLevel!=2 || !PrepareIncidentWorld()){aborted=done=true;return dt;}
  incidentWorldPrepared=true;stableMs=0;return dt;
 }
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
 // Start road drives on the road nearest the already loaded player, rather
 // than steering out of a parked driveway through buildings. Other captures
 // retain nearby parked cars. This placement happens before warmup/capture;
 // the measured drive still uses normal inputs, traffic and collision physics.
 Vehicle* setupCar=GetGameplayManager()->GetCurrentVehicle();
 avatar->GetPosition(placementPlayer);placementOriginalCar=setupCar->GetPosition();
 if(!casinoAnchor && (fixedRoadAnchor || (roadRoute && !portrait) ||
    (placementOriginalCar-placementPlayer).MagnitudeSqr()>40.0f*40.0f)){
  RoadSegment* segment=nullptr;float distance=0;
  // Paired captures use one explicit pre-capture road anchor. Loading timing
  // can otherwise move the idle player enough to choose another nearby road.
  const rmt::Vector reference=fixedRoadAnchor?roadAnchor:placementPlayer;
  GetIntersectManager()->FindClosestRoad(reference,30.0f,segment,distance);
  if(!segment){aborted=done=true;return dt;}
  rmt::Vector a,b,facing;segment->GetPosition(0,.5f,&a);segment->GetPosition(1,.5f,&b);
  const rmt::Vector edge=b-a;const float length=edge.MagnitudeSqr();
  float fraction=length>0?(reference-a).Dot(edge)/length:.5f;
  if(fraction<0)fraction=0;if(fraction>1)fraction=1;
  rmt::Vector position;segment->GetPosition(fraction,.5f,&position);
  avatar->GetCharacter()->GetFacing(facing);
  // A private direction control makes repeated routes independent of tiny
  // changes in the idle player's heading before automatic vehicle entry.
  const float alignment=roadDirection==3?-edge.x:edge.x*facing.x+edge.z*facing.z;
  const float sign=roadDirection==1?1.0f:roadDirection==2?-1.0f:alignment>=0?1.0f:-1.0f;
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
 // Match the game's normal instantaneous-entry path: placement alone does not
 // notify SoundAvatar or release the on-foot sound ducking policy.
 if(!portrait)GetEventManager()->TriggerEvent(EVENT_GETINTOVEHICLE_END,avatar->GetCharacter());
 avatar->SetCameraTargetToVehicle(true);
 GetSuperCamManager()->GetSCC(0)->SelectSuperCam(SuperCam::FAR_FOLLOW_CAM,SuperCamCentral::CUT|SuperCamCentral::FORCE);
 if(portrait){
  GetAvatarManager()->PutCharacterOnGround(avatar->GetCharacter(),GetGameplayManager()->GetCurrentVehicle());
  avatar->SetCameraTargetToCharacter(true);
  Character* character=avatar->GetCharacter();
  rmt::Vector position;character->GetPosition(position);
  if(suspensionProbe){
   Vehicle* car=GetGameplayManager()->GetCurrentVehicle();
   position=car->GetPosition();const rmt::Vector side=car->GetFacing();
   position.x+=3.0f*side.z;position.z-=3.0f*side.x;
  }
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
 if(WorldStreaming::incidentRoute) {
  if(portraitLevel!=2){aborted=done=true;return dt;}
  rmt::Vector continuation[512];const unsigned continuationCount=roadPointCount;
  for(unsigned n=0;n<continuationCount;++n)continuation[n]=roadPoints[n];
  // Preserve the observed westward approach rather than allowing the road
  // graph's branch choice to take a detour before the recorded failure.
  const rmt::Vector firstAnchors[]={{111.78008f,6.65482f,-1.76482f},
      {63.50772f,6.65482f,-5.18919f},{-75.89345f,6.65482f,-18.26584f},
      {-136.80171f,6.65482f,-4.13719f},{-225.f,6.799f,10.f}};
  rmt::Vector anchors[8];unsigned anchorCount=5;
  for(unsigned n=0;n<5;++n)anchors[n]=firstAnchors[n];
  if(WorldStreaming::incidentRoute>=2) {
   const rmt::Vector recorded[]={{111.68709f,6.65f,-25.60642f},{63.36959f,6.65f,-5.61141f},
       {-75.75313f,6.65f,-16.89366f},{-142.44412f,6.65f,-10.15073f},
       {-190.52652f,6.8f,3.65597f},{-242.33736f,6.8f,25.12462f},
       {-237.47864f,7.0459f,121.82870f},{-237.72217f,6.83887f,128.11456f}};
   anchorCount=8;for(unsigned n=0;n<8;++n)anchors[n]=recorded[n];
  }
  roadPointCount=1;roadPoints[0]=GetGameplayManager()->GetCurrentVehicle()->GetPosition();
  roadPlannedDistance=0.f;
  if(WorldStreaming::incidentRoute==3){
   // A northbound excursion crosses the recorded Z2 dump gate, then returns
   // through the recorded demand route. Both variants start with the same art.
   const rmt::Vector prefix[]={{137.23523f,6.65f,210.f},{137.23523f,6.65f,192.18851f}};
   for(const auto& point:prefix){roadPlannedDistance+=std::sqrt((point-roadPoints[roadPointCount-1]).MagnitudeSqr());roadPoints[roadPointCount++]=point;}
  }
  for(unsigned n=0;n<anchorCount;++n){const auto& anchor=anchors[n];roadPlannedDistance+=std::sqrt((anchor-roadPoints[roadPointCount-1]).MagnitudeSqr());roadPoints[roadPointCount++]=anchor;}
  if(!WorldStreaming::fly) {
   unsigned join=0;float nearest=1e30f;
   for(unsigned n=0;n<continuationCount;++n){const float distance=(continuation[n]-anchors[anchorCount-1]).MagnitudeSqr();if(distance<nearest){nearest=distance;join=n;}}
   if(nearest>40.f*40.f){aborted=done=true;return dt;}
   for(unsigned n=join+1;n<continuationCount && roadPointCount<512;++n){
    roadPlannedDistance+=std::sqrt((continuation[n]-roadPoints[roadPointCount-1]).MagnitudeSqr());roadPoints[roadPointCount++]=continuation[n];
   }
  }
  roadPoint=1;
 }

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
 if(missionNpcProbe && !MissionNPCProbeStep(elapsedMs)){aborted=done=true;return dt;}
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
 if(WorldStreaming::fly)UpdateStreamingFlight(avatar,dt);
 if(roadRoute && !portrait && !WorldStreaming::fly){if(elapsedMs<Old3dsRecurring::stationaryMs){roadGas=0;roadBrake=0;roadSteer=0;}else UpdateRoadInput(GetGameplayManager()->GetCurrentVehicle(),dt);}
 if(aborted)return dt;
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
 // This diagnostic changes only equivalent lookup strategies. Randomization
 // stays outside simulation RNG and records treatment on the same moving frame.
 if(residencyCpuProbe){
  unsigned policy=frame*0x7feb352du;policy^=policy>>16;policy*=0x846ca68bu;policy^=policy>>16;
  WorldResidency::CacheLookup((policy&1)!=0);
 }
 current.residencyPolicy=WorldResidency::CachePolicy();
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
 WorldStreaming::BeginFrame(routeMs,roadDistance);
 if(WorldStreaming::fly && WorldStreaming::flightCpuUs) {
  // Model the measured extra main-thread work of the captured real drive.
  // This reservation is stress diagnosis, never gameplay performance evidence.
  const u64 start=svcGetSystemTick(),budget=u64(WorldStreaming::flightCpuUs)*SYSCLOCK_ARM11/1000000;
  while(svcGetSystemTick()-start<budget){}
 }

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
static void UpdateStreamingFlight(Avatar* avatar,unsigned dt)
{
 static rmt::Vector position;
 if(!WorldStreaming::flightActive)position=GetGameplayManager()->GetCurrentVehicle()->GetPosition();
 float remaining=frame?WorldStreaming::flightSpeed*dt*.001f:0.f;
 rmt::Vector heading(0.f,0.f,1.f);
 while(roadPoint<roadPointCount) {
  const rmt::Vector delta=roadPoints[roadPoint]-position;const float length=std::sqrt(delta.MagnitudeSqr());
  if(length<.01f){++roadPoint;continue;}
  heading=delta*(1.f/length);
  if(remaining<length){position+=heading*remaining;roadDistance+=remaining;break;}
  position=roadPoints[roadPoint++];remaining-=length;roadDistance+=length;
 }
 if(roadPoint>=roadPointCount) {
  if(!WorldStreaming::incidentRoute){roadPathExhausted=true;aborted=done=true;return;}
  heading.Set(WorldStreaming::incidentRoute==2?0.f:-1.f,0.f,WorldStreaming::incidentRoute==2?1.f:0.f);
 }

 WorldStreaming::flightActive=true;WorldStreaming::flightAvatar=avatar;
 for(unsigned i=0;i<3;++i){WorldStreaming::flightPosition[i]=position[i];WorldStreaming::flightVelocity[i]=heading[i]*(roadPoint<roadPointCount?WorldStreaming::flightSpeed:0.f);}
 SuperCamCentral* central=GetSuperCamManager()->GetSCC(0);
 StaticCam* camera=static_cast<StaticCam*>(central->GetSuperCam(SuperCam::STATIC_CAM));
 if(!camera){camera=new StaticCam();central->RegisterSuperCam(camera);}
 // Four fixed bearings inspect forward, right, behind and left. Camera turns
 // never change the streaming origin or wait for incomplete resource requests.
 const unsigned bearing=WorldStreaming::cameraSweep?(elapsedMs/3000)%4:0;
 rmt::Vector view=heading;
 if(bearing==1)view.Set(heading.z,0.f,-heading.x);
 else if(bearing==2)view=-heading;
 else if(bearing==3)view.Set(-heading.z,0.f,heading.x);
 camera->SetTarget(GetGameplayManager()->GetCurrentVehicle());camera->SetTracking(false);
 camera->SetPosition(position+rmt::Vector(0.f,3.f,0.f)-view*4.f);
 camera->SetTargetOffset(position+rmt::Vector(0.f,1.f,0.f)+view*20.f);
 camera->SetTargetLag(1.f);camera->SetMaxFOV(WorldStreaming::flightFov);camera->SetFOV(WorldStreaming::flightFov);
 if(central->GetActiveSuperCam()!=camera)central->SelectSuperCam(camera,SuperCamCentral::CUT|SuperCamCentral::FORCE);
}
static unsigned hashFloat(unsigned h,float f){unsigned v;memcpy(&v,&f,4);return (h^v)*16777619u;}
bool End(u64 work,int context){
 if(irisCapture){
  auto* manager=GetGuiSystem()->GetInGameManager();auto* window=manager?manager->GetCurrentWindow():nullptr;
  const bool closed=window && window->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_IRIS_WIPE && window->IsRunning() && CGuiScreenIrisWipe::IsIrisClosed();
  if(closed){if(++irisClosedFrames==3 && irisCaptures<8){WorldStreaming::MarkerImage();++irisCaptures;printf("NATIVE_IRIS_CAPTURE index=%u closed_frames=%u source=display_capture performance_capture=0\n",irisCaptures,irisClosedFrames);}}
  else irisClosedFrames=0;
 }
 if(WorldResidency::Failed() || GetRenderManager()->ZoneQueueFailed()){aborted=done=true;return true;}
 if(WorldStreaming::passive)return PassiveEnd(work,context);
 Old3dsRecurring::End();
#ifdef SRR2_OLD3DS_COARSE_POOLS
 Old3dsPools::End();
#endif
#ifdef SRR2_OLD3DS_MAIN_PROFILE
 Old3dsMainProfile::EndFrame(work);
#endif
 if(reloadSampling && reloadStats && context==CONTEXT_GAMEPLAY && reloadPhase<=reloadPlan.count){
  auto* gameplay=GetGameplayManager();auto* mission=gameplay?gameplay->GetCurrentMission():nullptr;
  reloadStats[reloadPhase].Sample(reloadPeriod,work,SYSCLOCK_ARM11,linearSpaceFree(),
      GetLoadingManager()->GetNumCurrentRequests(),GetVehicleCentral()->GetNumVehicles(),mission?mission->GetCurrentStageIndex():-1);
 }
 if(!collecting)return aborted || (done && reload && reloadPhase==reloadPlan.count+1 && !hold);Checkpoint("leave",-1);u64 overhead=svcGetSystemTick();collecting=false;
#if defined(SRR2_OLD3DS_VEHICLE_PROFILE) || defined(SRR2_OLD3DS_NATIVE_BENCH)
 Old3dsVehicleProfile::CaptureEnabled()=false;
#endif
 WorldStreaming::EndFrame();
 PedestrianManager::GetInstance()->GetPopulationCounts(current.activePeds,current.frustumPeds);
 tPointCamera* streamCamera=GetSuperCamManager()->GetSCC(0)->GetCamera();
 const rmt::Vector streamPosition=streamCamera->GetPosition(),streamTarget=streamCamera->GetTarget();
 for(unsigned i=0;i<3;++i){current.cameraPosition[i]=streamPosition[i];current.cameraTarget[i]=streamTarget[i];}
 const unsigned activeTraffic=unsigned(TrafficManager::GetInstance()->GetNumActiveTraffic());
 ++trafficFrames;trafficActiveTotal+=activeTraffic;trafficVisibleTotal+=trafficVisible;
 if(activeTraffic>trafficActiveMaximum)trafficActiveMaximum=activeTraffic;
 if(trafficVisible>trafficVisibleMaximum)trafficVisibleMaximum=trafficVisible;
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
 if(suspensionProbe){
  current.physicsLimit=lastSuspensionStep;
  Vehicle* car=GetGameplayManager()->GetCurrentVehicle();
  if(car && car->GetSimState()){
   const rmt::Vector p=car->GetPosition();for(unsigned i=0;i<3;++i)current.carPosition[i]=p[i];
   current.carVertical=car->GetSimState()->GetLinearVelocity().y;
   current.carAngular=car->GetSimState()->GetAngularVelocity().Magnitude();
   current.carInPhysics=car->mUsingInCarPhysics;current.carRest=car->mAtRestAsFarAsTriggersAreConcerned;
  }
 }
 current.behavior=behaviorHash;current.gpu=C3D_GetDrawingTime();unsigned us=(unsigned)(work*1000000ULL/SYSCLOCK_ARM11);unsigned bin=us/250;if(bin>=Bins)bin=Bins-1;++hist[bin];
 ++samples;totalWork+=work;if(us>33333)++misses;if(us>maximum)maximum=us;
 records[recordCount++%recordCapacity]=current;previousCaptured=true;if(recordCount>recordCapacity)++overwritten;
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
static void DumpVisualEye(FILE* f,gfx3dSide_t eye,const char* tag,gfxScreen_t screen=GFX_TOP,bool afterRoute=true)
{
 u16 width=0,height=0;
 const GSPGPU_FramebufferFormat format=gfxGetScreenFormat(screen);
 unsigned char* pixels=gfxGetFramebuffer(screen,eye,&width,&height);
 const unsigned bytes=unsigned(width)*height*3;
 const bool valid=pixels && format==GSP_BGR8_OES && width==240 && (screen==GFX_BOTTOM?height==320:(height==400 || height==800)) &&
     R_SUCCEEDED(GSPGPU_InvalidateDataCache(pixels,bytes));
 fprintf(f,"%s enabled=1 valid=%u screen=%s raw_width=%u raw_height=%u format=%u back_buffer=1 age_unknown=1 after_route=%u performance_capture=0\n",tag,valid,screen==GFX_BOTTOM?"bottom":"top",width,height,unsigned(format),unsigned(afterRoute));
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
void Dump(){if(WorldStreaming::passive){DumpPassive();return;}if(!enabled)return;
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
 fprintf(f,"NATIVE_ADPCM_FAST enabled=%u calls=%u fallbacks=%u predictor_order_unchanged=%u budget_calls=%u budget_predictors=3 callbacks_deferred=0\n",g_old3dsAdpcmFast,g_old3dsAdpcmFastCalls,g_old3dsAdpcmFastFallbacks,g_old3dsAdpcmBudgetCalls==0,g_old3dsAdpcmBudgetCalls);
 fprintf(f,"NATIVE_ADPCM_VERIFY enabled=%u verified=%u mismatches=%u allocation_failures=%u performance_capture=%u\n",g_old3dsAdpcmVerify,g_old3dsAdpcmVerified,g_old3dsAdpcmMismatches,g_old3dsAdpcmVerifyAllocFailures,!g_old3dsAdpcmVerify);
 fprintf(f,"NATIVE_ADPCM_PRUNE enabled=%u exact_predictor_order=1 callbacks_deferred=0\n",g_old3dsAdpcmPrune);
 fprintf(f,"NATIVE_TRAFFIC_FACING enabled=%u\n",g_old3dsTrafficFacingOnly);
 fprintf(f,"NATIVE_SERVICE_PROFILE enabled=%u in_memory=1 post_capture_dump=1\n",serviceProfile);
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
 fprintf(f,"NATIVE_SKIN_LOAD_SLICES enabled=%u checkpoints=%u vertex_interval=64 whole_session=1 unchanged_vertex_bytes=1\n",g_picaSkinPrepareYield,g_picaSkinPrepareCheckpoints);
#endif
 radLoadDumpWorkerSlices(f);
#ifdef SRR2_OLD3DS_LEVEL_CACHE
 picaNativeLevel::Finish();picaNativeLevel::Dump(f);
#endif
 p3dDumpNativeReader(f);
 old3dsDumpParkedRemoval(f);
 srr2DumpWorldOrder(f);
#ifdef SRR2_OLD3DS_SPRITE_ATLAS
 fprintf(f,"NATIVE_SPRITE_ATLAS enabled=%u loaded=%u applied=%u batched=%u avoided_tile_draws=%u source_geometry_unchanged=1\n",p3dSpriteAtlas::enabled,p3dSpriteAtlas::loaded,p3dSpriteAtlas::applied,p3dSpriteAtlas::batched,p3dSpriteAtlas::avoided);
 fprintf(f,"NATIVE_SPRITE_ATLAS_REJECTIONS textures=%u shape=%u tile=%u format=%u\n",p3dSpriteAtlas::texturesLoaded,p3dSpriteAtlas::rejectShape,p3dSpriteAtlas::rejectTile,p3dSpriteAtlas::rejectFormat);
 fprintf(f,"NATIVE_SPRITE_ATLAS_MIPS enabled=%u pixel_review_required=%u\n",p3dSpriteAtlas::allowMipmaps,p3dSpriteAtlas::allowMipmaps);
 p3dSpriteAtlas::DumpSprites(f);
#endif
 WorldResidency::Dump(f);GetRenderManager()->DumpZoneQueue(f);WorldStreaming::Dump(f,runIdentity);
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
 fprintf(f,"NATIVE_BUILD id=%s scenario=%s mode=%s seed=%u step_ms=%u seconds=%u gate=%u selected_render=%u selected_physics=%u aborted=%u detail=%u validate=%u hold=%u render=%u physics=%u record_bytes=%u capacity=%u overwritten=%u scope_pair_ns=%.3f\n",SRR2_OLD3DS_BUILD_ID,portrait?"character-portrait":WorldStreaming::manual?"loaded-level-manual-v1":roadRoute?"loaded-level-road-v3":"loaded-level-drive-v2",replay?"replay":"realtime",seed,step,seconds,0u,0u,0u,aborted,detail,validation,hold,
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
(unsigned)(recordCapacity*sizeof(Record)),recordCapacity,overwritten,(double)scopeCalibration*1e9/SYSCLOCK_ARM11/4096);
 fprintf(f,"NATIVE_AUTOMATION diagnostic=%u resident=%u uniforms=%u context_mode=%u source=automation_only clock_complete=%u validation_instrumented=%u gpu_last_is_unassociated=1\n",diagnostic,residentShaders,nativeUniforms,nativeContextMode,done && !aborted,
 0u
 );
 fprintf(f,"NATIVE_FONT_WORK enabled=%u prepared_lines=%u prepared_glyphs=%u long_lines=%u capacity=%u same_vertices=1 same_texture_order=1 same_spacing_pass=1 whole_session=1\n",p3dFontWork::enabled,p3dFontWork::preparedLines,p3dFontWork::preparedGlyphs,p3dFontWork::longLines,p3dFontWork::Capacity);
 fprintf(f,"NATIVE_LOADER_SLICE override_us=%u gameplay_only=1 loading_policy_unchanged=1 geometry_io_unchanged=1\n",loaderSliceUs);
 fprintf(f,"NATIVE_GRAPHICS model=%u quality=%u stereo=%u width=%u height=240 target_fps=%u\n",unsigned(Graphics3DS::SystemModel()),Graphics3DS::Quality(),Graphics3DS::Stereo(),Graphics3DS::RenderWidth(),Graphics3DS::TargetFPS());
 fprintf(f,"NATIVE_QUALITY mode=%u draw_distance=%.1f ambient_traffic=%u ambient_pedestrians=%u physics_step_ms=%u audio_predictors=%u particle_emission_scale=%.2f distant_pedestrian_actions=%u flat_lighting=%u shadow_distance=%.1f mission_simulation=full\n",Old3dsPerformance::mode,Old3dsPerformance::Enabled()?Old3dsPerformance::DrawDistance():200.0f,Old3dsPerformance::Enabled()?AmbientTrafficLimit(Old3dsPerformance::TrafficLimit()):5u,Old3dsPerformance::Enabled()?Old3dsPerformance::PedestrianLimit():7u,Old3dsPerformance::PhysicsStepMilliseconds(),Old3dsPerformance::ReducedAudioSearch()?3u:8u,Old3dsPerformance::ParticleEmissionScale(),Old3dsPerformance::DistantPedestrianActions(),Old3dsPerformance::FlatLighting(),Old3dsPerformance::Enabled()?0.0f:200.0f);
 fprintf(f,"NATIVE_AMBIENT_TRAFFIC limit=%u frames=%u active_total=%u visible_total=%u active_maximum=%u visible_maximum=%u scope=measured_route cpu_scene_traversals_added=0\n",AmbientTrafficLimit(Old3dsPerformance::TrafficLimit()),trafficFrames,trafficActiveTotal,trafficVisibleTotal,trafficActiveMaximum,trafficVisibleMaximum);
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
 unsigned fogModeCalls,fogModeSkipped,fogColorCalls,fogColorSkipped,fogFlushes;
 picaNativeFogStats(&fogModeCalls,&fogModeSkipped,&fogColorCalls,&fogColorSkipped,&fogFlushes);
 fprintf(f,"NATIVE_FOG_STATE mode_calls=%u mode_skipped=%u color_calls=%u color_skipped=%u flushes=%u scope=whole_session actual_transitions_preserved=1\n",fogModeCalls,fogModeSkipped,fogColorCalls,fogColorSkipped,fogFlushes);
#endif


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

 // Post-capture actor state rules out model-retention work as a hidden cost.
 // No actor iteration or timing instrumentation is added to measured frames.
 {unsigned actors=0,farActors=0,farReady=0;rmt::Vector observer;
  auto* avatar=GetAvatarManager()->GetAvatarForPlayer(0);
  if(avatar){avatar->GetPosition(observer);
   for(unsigned i=0;i<unsigned(CharacterManager::GetMaxCharacters());++i){
    Character* character=GetCharacterManager()->GetCharacter(i);
    if(!character || character->GetRole()!=Character::ROLE_MISSION)continue;
    ++actors;rmt::Vector position;character->GetPosition(position);
    if((position-observer).MagnitudeSqr()>150.f*150.f){++farActors;if(character->NativeBenchModelReady())++farReady;}
   }
  }
  fprintf(f,"NATIVE_MISSION_MODELS actors=%u far_actors=%u far_real_models=%u post_capture=1 measured_frame_work_added=0\n",actors,farActors,farReady);
 }

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
 fprintf(f,"NATIVE_VISUAL_FEATURES mask=%u load_prepared_form=1 reflection_draws_whole_session=%u scenery_draws_whole_session=%u\n",
     picaVisual::features,picaVisual::reflectionDraws,picaVisual::sceneryDraws);
 fprintf(f,"NATIVE_CONTACT_SHADOWS enabled=%u queued=%u batches=%u instances=%u overflow=%u command_skips=%u peak_instances=%u scope=whole_session cpu_scene_walks=0 new_ground_rays=0\n",
     picaVisual::Enabled(picaVisual::ContactShadows),picaContact::queued,picaContact::batches,picaContact::drawn,picaContact::overflow,picaContact::commandSkips,picaContact::peakInstances);
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
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
 fprintf(f,"NATIVE_CHARACTER_OCCLUSION enabled=%u groups=%u vertices=%u misses=%u load_only=1\n",
  picaVisual::Enabled(picaVisual::Occlusion),picaCharacterOcclusion::groups,picaCharacterOcclusion::vertices,picaCharacterOcclusion::misses);
#endif
 fprintf(f,"NATIVE_PERIOD samples=%u mean_us=%.3f fps=%.6f\n",periodSamples,periodSamples?(double)periodSum*1e6/SYSCLOCK_ARM11/periodSamples:0,periodSum?(double)periodSamples*SYSCLOCK_ARM11/periodSum:0);
 fprintf(f,"NATIVE_LOAD_WINDOW placement_valid=%u boot_to_placement_us=%.3f includes_initialization_and_loading=1 includes_realtime_stability_ms=%u excludes_netloader_transfer=1\n",
     placementTick!=0,placementTick?(placementTick-initializationTick)*1000000.0/SYSCLOCK_ARM11:0,replay?0u:3000u);
 fprintf(f,"STREAM_FLIGHT_STRESS cpu_reserve_us=%u fov=%.6f modeled_workload=1 gameplay_performance=0\n",WorldStreaming::flightCpuUs,WorldStreaming::flightFov);
 fprintf(f,"STREAM_REVISIT_SETUP requested=%u world_prepared=%u before_warmup=1 resource_barriers_during_traversal=0\n",WorldStreaming::incidentRoute==3,incidentWorldPrepared);
 fprintf(f,"STREAM_FLIGHT enabled=%u speed_mps=%.3f collisions=0 focus_and_camera=1 gameplay_performance=0 incident_route=%u idle_loader=%u\n",WorldStreaming::fly,WorldStreaming::flightSpeed,WorldStreaming::incidentRoute,WorldStreaming::idleLoader);
 fprintf(f,"NATIVE_ROUTE road=%u points=%u point=%u distance=%.3f recoveries=%u planned_distance=%.3f built_roads=%u path_exhausted=%u input_only=%u\n",roadRoute,roadPointCount,roadPoint,roadDistance,roadRecoveries,roadPlannedDistance,roadBuiltCount,roadPathExhausted,!WorldStreaming::fly);
 for(unsigned i=0;i<roadPointCount;++i)
  fprintf(f,"STREAM_WAYPOINT index=%u x=%.5f y=%.5f z=%.5f\n",i,roadPoints[i].x,roadPoints[i].y,roadPoints[i].z);
 fprintf(f,"NATIVE_ROUTE_SETUP direction_override=%u initial_facing_radians=%.8f\n",roadDirection,GetGameplayManager()->GetCurrentVehicle()?GetGameplayManager()->GetCurrentVehicle()->mResetFacingRadians:0.0f);
 fprintf(f,"NATIVE_ROUTE_ANCHOR requested=%u x=%.5f y=%.5f z=%.5f before_capture=1\n",fixedRoadAnchor,roadAnchor.x,roadAnchor.y,roadAnchor.z);
 fprintf(f,"NATIVE_REPLAY drains=%u timeouts=%u reload=%u reload_phase=%u reload_dispatches=%u reload_observed_levels=%u\n",replayDrains,drainTimeouts,reload,reloadPhase,reloadDispatches,reloadObservedLevels);
 fprintf(f,"NATIVE_IRIS_AUDIT requested=%u captures=%u performance_capture=0\n",irisCapture,irisCaptures);
 fprintf(f,"NATIVE_RELOAD_PLAN custom=%u destinations=%u dwell_ms=%u watchdog_seconds=%u\n",reloadPlan.custom,reloadPlan.count,reloadPlan.dwellMs,reloadPlan.WatchdogSeconds());
 if(reloadStats)for(unsigned i=1;i<=reloadPlan.count;++i){const auto& stats=reloadStats[i];
  fprintf(f,"NATIVE_RELOAD_FRAMES phase=%u level=%u mission=%u entered=%u frames=%u fps=%.4f mean_work_ms=%.4f p95_upper_ms=%.3f p99_upper_ms=%.3f max_ms=%.3f over_50ms=%u linear_min=%u resets=%u teleports=%u interiors=%u mission_active=%u streaming_frames=%u vehicles_peak=%u stage_first=%d stage_last=%d stage_maximum=%d stereo_frames=%u stereo_segments=%u resident_expected=%u resident_bound=%u resident_bad=%u resident_wanted=%08x histogram_width_ms=2 memory_instrumented=%u\n",
   i,reloadPlan.levels[i-1]+1,reloadPlan.MissionNumber(i-1),reloadEntered[i],stats.samples,
   stats.period?double(stats.samples)*SYSCLOCK_ARM11/stats.period:0.,stats.samples?double(stats.work)*1000./SYSCLOCK_ARM11/stats.samples:0.,
   stats.PercentileUs(95)/1000.,stats.PercentileUs(99)/1000.,stats.maximumUs/1000.,stats.over50,stats.samples?stats.linearMinimum:0,stats.resets,stats.teleports,stats.interiors,stats.missionActive,stats.streamingFrames,stats.vehiclesPeak,stats.stageFirst,stats.stageLast,stats.stageMaximum,stats.stereoFrames,stats.stereoSegments,stats.residentExpected,stats.residentBound,stats.residentBad,stats.residentWanted,memoryDiagnostics);
 }

 fprintf(f,"NATIVE_MISSION_NPC_PROBE requested=%u far_ready=%u arrived=%u removed=%u actors=3 performance_capture=0 failure=%s ground_rejects=%u\n",missionNpcProbe,missionProbeFarReady,missionProbeArrived,missionProbeRemoved,missionProbeFailure,missionProbeGroundRejects);
 fprintf(f,"NATIVE_SUSPENSION_PROBE enabled=%u phase_ms=10000 alternate_max_ms=25 moving_performance_evidence=0\n",suspensionProbe);
 fprintf(f,"NATIVE_RESIDENCY_CPU_PROBE randomized=%u policies=2 simulation_rng_unchanged=1 geometry_unchanged=1 per_object_timers=0\n",residencyCpuProbe);
 fprintf(f,"NATIVE_VERIFY calls=%u failures=%u\n",verifyAll,verifyAllFailures);
 fprintf(f,"NATIVE_COUNTERS 0=timed_scope_calls 1=adpcm_clips 2=adpcm_pcm_samples\n");
 fprintf(f,"NATIVE_SUMMARY frames=%u mean_us=%.3f max_us=%u deadline_misses=%u bookkeeping_us=%.3f complete=%u\n",samples,samples?(double)totalWork*1000000/SYSCLOCK_ARM11/samples:0,maximum,misses,(double)traceTicks*1000000/SYSCLOCK_ARM11,done && !aborted);
 for(unsigned i=0;i<Bins;++i)if(hist[i])fprintf(f,"NATIVE_HIST low_us=%u count=%u\n",i*250,hist[i]);
 for(unsigned i=recordCount>recordCapacity?recordCount-recordCapacity:0;i<recordCount;++i){const Record&r=records[i%recordCapacity];
 fprintf(f,"NATIVE_FRAME native_context=%u route_ms=%u pairs=%u areas=%u entities=%u frame=%u context=%u elapsed=%u real_elapsed=%u period_us=%.3f work_us=%.3f substeps=%u draws=%u vertices=%u linear=%u loading=%u x=%.6f y=%.6f z=%.6f speed=%.5f incar=%u check=%08x behavior=%08x gpu_last_ms=%.4f",r.nativeContext,r.route,r.pairs,r.areas,r.entities,r.frame,r.context,r.elapsed,r.realElapsed,(double)r.period*1e6/SYSCLOCK_ARM11,(double)r.work*1000000/SYSCLOCK_ARM11,r.substeps,r.draws,r.vertices,r.linear,r.loading,r.position[0],r.position[1],r.position[2],r.speed,r.incar,r.check,r.behavior,r.gpu);
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 fprintf(f," bottom_map=%u bottom_draws=%u",r.bottomMapEnabled,r.bottomMapDraws);
#endif
 fprintf(f," heap_used=%u submit_begin=%u submit_end=%u",r.heapUsed,r.presentBegin,r.presentEnd);
 fprintf(f," residency_lookup_cache=%u",r.residencyPolicy&1);
 if(suspensionProbe)fprintf(f," car_x=%.6f car_y=%.6f car_z=%.6f car_vertical_mps=%.6f car_angular_rps=%.6f car_in_physics=%u car_rest=%u physics_limit_ms=%u suspension_phase=%u",
    r.carPosition[0],r.carPosition[1],r.carPosition[2],r.carVertical,r.carAngular,r.carInPhysics,r.carRest,r.physicsLimit,(r.route/10000u)&1u);
 fprintf(f," camera_x=%.5f camera_y=%.5f camera_z=%.5f target_x=%.5f target_y=%.5f target_z=%.5f active_peds=%u frustum_peds=%u",r.cameraPosition[0],r.cameraPosition[1],r.cameraPosition[2],r.cameraTarget[0],r.cameraTarget[1],r.cameraTarget[2],r.activePeds,r.frustumPeds);
 fprintf(f," skin_budget_groups=%u skin_budget_vertices=%u",r.skinBudgetFallbacks,r.skinBudgetVertices);
 for(unsigned j=0;j<Count;++j)fprintf(f," %s_us=%.3f %s_ex_us=%.3f",names[j],(double)r.stage[j]*1000000/SYSCLOCK_ARM11,names[j],(double)r.exclusive[j]*1000000/SYSCLOCK_ARM11);
 for(unsigned j=0;j<CountC;++j)fprintf(f," count_%u=%u",j,r.count[j]);fprintf(f,"\n");}
 #ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
 fprintf(f,"NATIVE_BOTTOM_POLICY mode=%u block_frames=24 randomized=%u policy_seed=%u simulation_rng_unchanged=1\n",bottomMapMode,bottomMapMode==2,bottomMapSeed);
 HandheldHudCadence::Dump(f);
 #endif
 Old3dsCadence::Dump(f);Old3dsMemoryDump(f);DumpVisual(f);fprintf(f,"NATIVE_CAPTURE_END frames=%u complete=%u\n",samples,done && !aborted && (!reload || reloadPhase==reloadPlan.count+1));fclose(f);
}
int Input(bool axis,int id,int original){
 if(!enabled||done)return original;if(!placed||warmMs<warmup*1000||done)return 0;
 if(portrait || suspensionProbe || missionNpcProbe)return 0;
 if(WorldStreaming::manual)return original;
 // Driving-only route: retain the established first 25 seconds of inputs.
 // No exit, walking, return navigation, reset or forced mount during capture.
 if(roadRoute){
  if(routeMs<Old3dsRecurring::stationaryMs)return !axis && id==SDL_CONTROLLER_BUTTON_Y;
  if(axis) {
   if(id==SDL_CONTROLLER_AXIS_LEFTX)return roadSteer;
   if(WorldStreaming::cameraSweep && id==SDL_CONTROLLER_AXIS_RIGHTX) {
    // Equal distance windows, independent of FPS: brief looks to both sides.
    const unsigned phase=unsigned(roadDistance/60.f)%4;
    return phase==1?26000:phase==3?-26000:0;
   }
   return 0;
  }
  if(WorldStreaming::cameraSweep && id==SDL_CONTROLLER_BUTTON_DPAD_DOWN)
   return unsigned(roadDistance/60.f)%8==4;
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
