#ifdef SRR2_OLD3DS_PREPARED_BACKEND
#include <pddi/pica/preparedbackend.h>
#endif
#ifdef SRR2_OLD3DS_RENDER_WORKER
#include <pddi/pica/renderworker.h>
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
#include <pddi/pica/occlusionmetrics.h>
#endif
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
#include <pddi/pica/nativecontext.h>
#endif
#ifdef SRR2_OLD3DS_NATIVE_MATRIX
#include <radmath/nativematrix.hpp>
#endif
#include <main/nativebench.h>
#ifdef SRR2_OLD3DS_PARALLEL_COLLISION
#include <simcollision/parallelcollisionmetrics.h>
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
#include <presentation/gui/guisystem.h>
#include <presentation/gui/ingame/guimanageringame.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#ifdef SRR2_OLD3DS_SUBMIT_PROFILE
extern "C" void nativeBenchDumpSubmissionProfile(FILE*);
#endif
namespace NativeBench {
bool detail=false,collecting=false,validation=false;
static unsigned behaviorHash=2166136261u;
void Hash(unsigned v){if(collecting && validation)behaviorHash=(behaviorHash^v)*16777619u;}
void HashFloat(float f){unsigned v;memcpy(&v,&f,4);Hash(v);}
unsigned counters[CountC];
static const unsigned Capacity=2048, Bins=2048;
struct Record {
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
 unsigned preparedWorkerDraws,preparedMainDraws,preparedJobs,preparedWaitTicks,preparedCommitTicks,preparedComparisons,preparedMismatches;
 unsigned preparedMode,preparedMaterials,preparedUniforms,preparedGeometry,preparedSkin,preparedViews,preparedLights,preparedMaterialUniforms,preparedFlushes,preparedBatchedDraws,preparedBatchTicks;
#endif
#ifdef SRR2_OLD3DS_RENDER_WORKER
 unsigned workerMode,workerContexts,mainContexts,workerFences,workerWait;
#endif
#ifdef SRR2_OLD3DS_PARALLEL_COLLISION
 unsigned parallelMode,parallelWorker,parallelMain,parallelCalls,parallelTicks;
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
 unsigned visibilityMode,visibilityTested,visibilityHidden,visibilitySkipped,visibilityVertices;
#endif
 unsigned nativeContext, frame, elapsed, context, period, realElapsed, stage[Count],exclusive[Count],count[CountC];
 unsigned work,draws,vertices,linear,loading,substeps,check,behavior;
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
static bool hold=false,reload=false;
static unsigned diagnostic=0;
static bool residentShaders=false;
static bool nativeUniforms=false;
static unsigned reloadPhase=0,reloadMs=0;
static bool enabled=true,replay=false,placed=false,done=false,aborted=false;
static u64 benchmarkStart;
static unsigned nativeContextMode;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
static unsigned preparedMode=0;
static picaPreparedBackendStats preparedStart;
#endif
#ifdef SRR2_OLD3DS_RENDER_WORKER
static unsigned renderWorkerMode=2;
static picaRenderWorkerStats workerStart;
#endif
#ifdef SRR2_OLD3DS_PARALLEL_COLLISION
static unsigned parallelMode=2;
static Old3dsParallelCollisionStats parallelStart;
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
static unsigned visibilityMode=2;
static picaVisibilityStats visibilityStart;
#endif
static unsigned stableMs, warmMs, replayDrains, drainTimeouts, pendingAtBegin;
static Scope* top;
static const char* names[Count]={"update","render","stream"};
void Add(Counter c,unsigned n){if(collecting)counters[c]+=n;}
void Scope::BeginScope(Stage s){stage=s;child=0;++counters[ScopeCalls];parent=top;top=this;start=svcGetSystemTick();}
void Scope::EndScope(){if(!start)return;u64 end=svcGetSystemTick(),dt=end-start;
 current.stage[stage]+=(unsigned)dt;current.exclusive[stage]+=(unsigned)(dt-child);
 if(parent)parent->child+=dt;top=parent;}
bool ResidentShaders(){return residentShaders;}
bool NativeUniforms(){return nativeUniforms;}
void Init(int argc,char** argv){
 FILE* f=fopen("native-automation.txt","r");
 // This development-only build defaults to the real-time route without SD setup.
 if(f){
 char key[64],value[64];while(fscanf(f,"%63[^=]=%63s\n",key,value)==2){
 if(!strcmp(key,"mode"))replay=!strcmp(value,"replay");
 else if(!strcmp(key,"seconds"))seconds=strtoul(value,0,10);
 else if(!strcmp(key,"warmup"))warmup=strtoul(value,0,10);
 else if(!strcmp(key,"seed"))seed=strtoul(value,0,10);
 else if(!strcmp(key,"step_ms"))step=strtoul(value,0,10);
 else if(!strcmp(key,"detail"))detail=atoi(value)!=0;
 // Historical native experiment validation is deliberately not restored.
 else if(!strcmp(key,"diagnostic"))diagnostic=strtoul(value,0,10);
 else if(!strcmp(key,"reload"))reload=atoi(value)!=0;
 else if(!strcmp(key,"hold"))hold=atoi(value)!=0;
 }fclose(f);}
 // Netloader arguments override the optional file without modifying the SD.
 // One exact executable can run matched reference/candidate investigations.
 for(int i=1;i<argc;++i){
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
  if(!strncmp(argv[i],"bench_prepared=",15)){preparedMode=strtoul(argv[i]+15,nullptr,10)%7;g_picaPreparedExecution=preparedMode==6?6:(preparedMode>=4?4:(preparedMode==3?3:1));g_picaPreparedBackendEnabled=preparedMode==1||preparedMode==3||preparedMode==4||preparedMode==5||preparedMode==6;continue;}
#endif
#ifdef SRR2_OLD3DS_RENDER_WORKER
  if(!strncmp(argv[i],"bench_worker=",13)){renderWorkerMode=strtoul(argv[i]+13,nullptr,10)%5;continue;}
#endif
  if(!strncmp(argv[i],"bench_mode=",11))replay=!strcmp(argv[i]+11,"replay");
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
  else if(!strncmp(argv[i],"bench_context=",14))nativeContextMode=strtoul(argv[i]+14,nullptr,10)%3;
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
  else if(!strncmp(argv[i],"bench_visibility=",17))visibilityMode=strtoul(argv[i]+17,nullptr,10)%4;
#endif
#ifdef SRR2_OLD3DS_PARALLEL_COLLISION
  else if(!strncmp(argv[i],"bench_parallel=",15))parallelMode=strtoul(argv[i]+15,nullptr,10)%4;
#endif
  else if(!strncmp(argv[i],"bench_resident=",15))residentShaders=atoi(argv[i]+15)!=0;
#ifdef SRR2_OLD3DS_NATIVE_MATRIX
  else if(!strncmp(argv[i],"bench_matrix=",13))g_nativeMatrixMode=strtoul(argv[i]+13,nullptr,10)%3;
#endif
  else if(!strncmp(argv[i],"bench_uniforms=",15))nativeUniforms=atoi(argv[i]+15)!=0;
  else if(!strncmp(argv[i],"bench_diagnostic=",17))diagnostic=strtoul(argv[i]+17,nullptr,10);
  else if(!strncmp(argv[i],"bench_step=",11))step=strtoul(argv[i]+11,nullptr,10);
 }
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
 // Validate gameplay rendering after automated placement. Loading/menu work uses
 // the established backend and does not pay doubled SDK comparison overhead.
 g_picaPreparedBackendEnabled=false;
#endif
 if(diagnostic>4)diagnostic=0;
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
 O3ExperimentExternal(diagnostic,false);
#endif
 if(seconds<5||seconds>25)seconds=25;if(warmup<3||warmup>60)warmup=8;if(step<1||step>100)step=67;
 // Use the existing normal heap after libctru startup, not additional BSS.
 records=static_cast<Record*>(calloc(Capacity,sizeof(Record)));
 if(!records){enabled=false;aborted=true;done=true;
  printf("NATIVE_AUTOMATION allocation_failed bytes=%u abort=1\n",unsigned(Capacity*sizeof(Record)));return;}
 bool savedDetail=detail;detail=true;collecting=true;u64 cal=svcGetSystemTick();for(unsigned i=0;i<4096;++i){Scope test(Update);}scopeCalibration=svcGetSystemTick()-cal;collecting=false;detail=savedDetail;
 char a[]="skipfe",b[]="notutorial",c[]="L2";CommandLineOptions::HandleOption(a);CommandLineOptions::HandleOption(b);CommandLineOptions::HandleOption(c);
}
unsigned Seed(unsigned fallback){return enabled?seed:fallback;}
unsigned Begin(unsigned elapsed,int context){
 u64 now=svcGetSystemTick();
 if(previousCaptured){unsigned ticks=(unsigned)(now-lastBeginTick);records[(recordCount-1)%Capacity].period=ticks;periodSum+=ticks;++periodSamples;previousCaptured=false;}
 lastBeginTick=now;
 collecting=false;
#ifdef SRR2_OLD3DS_NATIVE_MATRIX
 g_nativeMatrixCollecting=false;
#endif
 if(!enabled)return elapsed;
 if(done){
  if(reload && reloadPhase<3 && context==CONTEXT_GAMEPLAY && !GetLoadingManager()->IsLoading()){
   const int expected=reloadPhase==1?RenderEnums::L1:RenderEnums::L2;
   if((int)GetGameplayManager()->GetCurrentLevelIndex()==expected){
    reloadMs+=elapsed;
    if(reloadPhase==0 || reloadMs>=10000){
     if(reloadPhase==2)reloadPhase=3;
     else {GetGuiSystem()->GetInGameManager()->HandleMessage(GUI_MSG_QUIT_INGAME_FOR_RELOAD,
      reloadPhase==0?RenderEnums::L1:RenderEnums::L2,RenderEnums::M2);++reloadPhase;reloadMs=0;}
    }
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
 placed=true;
 }
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
 if(warmMs<warmup*1000){
  g_picaPreparedExecution=preparedMode==6?6:(preparedMode>=4?4:(preparedMode==3?3:1));
  g_picaPreparedBackendEnabled=preparedMode==1||preparedMode>=3;
  warmMs+=dt;return dt;
 }
#else
 if(warmMs<warmup*1000){warmMs+=dt;return dt;}
#endif
 memset(&current,0,sizeof(current));memset(counters,0,sizeof(counters));
 current.route=routeMs;current.frame=++frame;current.elapsed=dt;current.realElapsed=elapsed;current.context=context;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
 // Randomize blocks rather than individual draws. All fences finish this frame.
 unsigned preparedChoice=(frame/24)*0x9e3779b9u;preparedChoice^=preparedChoice>>16;preparedChoice*=0x85ebca6bu;preparedChoice^=preparedChoice>>13;
 g_picaPreparedExecution=preparedMode==6?6:(preparedMode>=4?4:(preparedMode==3?3:1));
 g_picaPreparedBackendEnabled=(preparedMode==2||preparedMode==5)?(preparedChoice&1)!=0:(preparedMode==1||preparedMode==3||preparedMode==4||preparedMode==6);
 current.preparedMode=g_picaPreparedBackendEnabled?g_picaPreparedExecution:0;picaPreparedBackendGetStats(&preparedStart);
#endif
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
 // Deterministic per-frame randomization avoids separate-run traffic confounding.
 unsigned choice=frame*0x9e3779b9u;choice^=choice>>16;choice*=0x85ebca6bu;choice^=choice>>13;
 g_picaNativeContextEnabled=nativeContextMode==1 || (nativeContextMode==2 && (choice&1));
 current.nativeContext=g_picaNativeContextEnabled;
#ifdef SRR2_OLD3DS_OCCLUSION
 g_picaVisibilityMode=visibilityMode==3 ? ((choice&1)?1:0) : visibilityMode;
 current.visibilityMode=g_picaVisibilityMode;
 picaVisibilityGetStats(&visibilityStart);
#endif
#endif
#ifdef SRR2_OLD3DS_PARALLEL_COLLISION
 unsigned parallelChoice=frame*0x9e3779b9u;parallelChoice^=parallelChoice>>16;parallelChoice*=0x85ebca6bu;parallelChoice^=parallelChoice>>13;
 g_old3dsParallelCollisionMode=parallelMode==3?((parallelChoice&1)?1:0):parallelMode;
 current.parallelMode=g_old3dsParallelCollisionMode;old3dsParallelCollisionGetStats(&parallelStart);
#endif
#ifdef SRR2_OLD3DS_RENDER_WORKER
 unsigned workerChoice=frame*0x9e3779b9u;workerChoice^=workerChoice>>16;workerChoice*=0x85ebca6bu;workerChoice^=workerChoice>>13;
 g_picaRenderWorkerMode=renderWorkerMode==4?((workerChoice&1)?3:0):renderWorkerMode;
 current.workerMode=g_picaRenderWorkerMode;picaRenderWorkerGetStats(&workerStart);
#endif
 routeMs=elapsedMs;elapsedMs+=dt;collecting=true;top=0;behaviorHash=2166136261u;
#ifdef SRR2_OLD3DS_NATIVE_MATRIX
 g_nativeMatrixCollecting=true;
#endif
 pairStart=g_old3dsCollisionPairs;areaStart=g_old3dsPhysicsAreas;entityStart=entityCalls();
 drawStart=g_old3dsDrawCalls;vertStart=g_old3dsDrawVerts;subStart=g_old3dsPhysicsSubsteps;
 return dt;
}
static unsigned hashFloat(unsigned h,float f){unsigned v;memcpy(&v,&f,4);return (h^v)*16777619u;}
bool End(u64 work,int context){
 if(!collecting)return aborted || (done && reload && reloadPhase==3 && !hold);u64 overhead=svcGetSystemTick();collecting=false;
#ifdef SRR2_OLD3DS_NATIVE_MATRIX
 g_nativeMatrixCollecting=false;
#endif
#ifdef SRR2_OLD3DS_OCCLUSION
 picaVisibilityStats visibilityEnd;picaVisibilityGetStats(&visibilityEnd);
 current.visibilityTested=visibilityEnd.tested-visibilityStart.tested;
 current.visibilityHidden=visibilityEnd.hidden-visibilityStart.hidden;
 current.visibilitySkipped=visibilityEnd.skipped-visibilityStart.skipped;
 current.visibilityVertices=visibilityEnd.skippedVertices-visibilityStart.skippedVertices;
#endif
#ifdef SRR2_OLD3DS_PARALLEL_COLLISION
 Old3dsParallelCollisionStats parallelEnd;old3dsParallelCollisionGetStats(&parallelEnd);
 current.parallelWorker=parallelEnd.workerAreas-parallelStart.workerAreas;
 current.parallelMain=parallelEnd.mainAreas-parallelStart.mainAreas;
 current.parallelCalls=parallelEnd.calls-parallelStart.calls;
 current.parallelTicks=parallelEnd.totalTicks-parallelStart.totalTicks;
#endif
 current.pairs=g_old3dsCollisionPairs-pairStart;current.areas=g_old3dsPhysicsAreas-areaStart;current.entities=entityCalls()-entityStart;
 current.work=(unsigned)work;current.context=context;current.draws=g_old3dsDrawCalls-drawStart;current.vertices=g_old3dsDrawVerts-vertStart;
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
 picaPreparedBackendStats preparedEnd;picaPreparedBackendGetStats(&preparedEnd);
 current.preparedMaterials=preparedEnd.materials-preparedStart.materials;
 current.preparedUniforms=preparedEnd.uniforms-preparedStart.uniforms;
 current.preparedGeometry=preparedEnd.geometryCaptures-preparedStart.geometryCaptures;
 current.preparedSkin=preparedEnd.skinCaptures-preparedStart.skinCaptures;
 current.preparedViews=preparedEnd.viewRecords-preparedStart.viewRecords;
 current.preparedLights=preparedEnd.lightRecords-preparedStart.lightRecords;
 current.preparedMaterialUniforms=preparedEnd.materialUniformRecords-preparedStart.materialUniformRecords;
 current.preparedFlushes=preparedEnd.batchFlushes-preparedStart.batchFlushes;
 current.preparedBatchedDraws=preparedEnd.batchedDraws-preparedStart.batchedDraws;
 current.preparedBatchTicks=(unsigned)(preparedEnd.batchTicks-preparedStart.batchTicks);
 current.preparedWorkerDraws=preparedEnd.workerDraws-preparedStart.workerDraws;current.preparedMainDraws=preparedEnd.mainDraws-preparedStart.mainDraws;
 current.preparedJobs=preparedEnd.workerJobs-preparedStart.workerJobs;
 current.preparedWaitTicks=(unsigned)(preparedEnd.workerWaitTicks-preparedStart.workerWaitTicks);current.preparedCommitTicks=(unsigned)(preparedEnd.workerCommitTicks-preparedStart.workerCommitTicks);
 current.preparedComparisons=preparedEnd.comparisons-preparedStart.comparisons;current.preparedMismatches=preparedEnd.mismatches-preparedStart.mismatches;
#endif
#ifdef SRR2_OLD3DS_RENDER_WORKER
 picaRenderWorkerStats workerEnd;picaRenderWorkerGetStats(&workerEnd);
 current.workerContexts=workerEnd.workerContexts-workerStart.workerContexts;
 current.mainContexts=workerEnd.mainContexts-workerStart.mainContexts;
 current.workerFences=workerEnd.fences-workerStart.fences;
 current.workerWait=(unsigned)(workerEnd.waitTicks-workerStart.waitTicks);
#endif
 current.linear=linearSpaceFree();current.loading=GetLoadingManager()->GetNumCurrentRequests()+pendingAtBegin;current.substeps=g_old3dsPhysicsSubsteps-subStart;
 memcpy(current.count,counters,sizeof(counters));Avatar* a=GetAvatarManager()->GetAvatarForPlayer(0);
 if(a){rmt::Vector p;a->GetPosition(p);current.position[0]=p.x;current.position[1]=p.y;current.position[2]=p.z;current.speed=a->GetSpeedMps();current.incar=a->IsInCar();
 rmt::Vector v;a->GetVelocity(v);unsigned h=2166136261u;h=hashFloat(h,p.x);h=hashFloat(h,p.y);h=hashFloat(h,p.z);h=hashFloat(h,v.x);h=hashFloat(h,v.y);h=hashFloat(h,v.z);current.check=h;}
 current.behavior=behaviorHash;current.gpu=C3D_GetDrawingTime();unsigned us=(unsigned)(work*1000000ULL/SYSCLOCK_ARM11);unsigned bin=us/250;if(bin>=Bins)bin=Bins-1;++hist[bin];
 ++samples;totalWork+=work;if(us>33333)++misses;if(us>maximum)maximum=us;
 records[recordCount++%Capacity]=current;previousCaptured=true;if(recordCount>Capacity)++overwritten;
 traceTicks+=svcGetSystemTick()-overhead;
 done=elapsedMs>=seconds*1000;return done && !hold && !reload;
}
void Dump(){if(!enabled)return;FILE*f=fopen("native-" SRR2_OLD3DS_BUILD_ID ".log","w");if(!f)return;
#ifdef SRR2_OLD3DS_SUBMIT_PROFILE
 nativeBenchDumpSubmissionProfile(f);
#endif
#ifdef SRR2_OLD3DS_NATIVE_MATRIX
 old3dsNativeMatrixDump(f);
#endif
 fprintf(f,"NATIVE_BUILD id=%s scenario=L2-drive-v1 mode=%s seed=%u step_ms=%u seconds=%u gate=%u selected_render=%u selected_physics=%u aborted=%u detail=%u validate=%u hold=%u render=%u physics=%u record_bytes=%u capacity=%u overwritten=%u scope_pair_ns=%.3f\n",SRR2_OLD3DS_BUILD_ID,replay?"replay":"realtime",seed,step,seconds,0u,0u,0u,aborted,detail,validation,hold,
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
 fprintf(f,"NATIVE_AUTOMATION diagnostic=%u resident=%u uniforms=%u context_mode=%u source=automation_only clock_complete=%u validation_instrumented=0 gpu_last_is_unassociated=1\n",diagnostic,residentShaders,nativeUniforms,nativeContextMode,done && !aborted);
#ifdef SRR2_OLD3DS_OCCLUSION
 picaVisibilityStats v;picaVisibilityGetStats(&v);
 fprintf(f,"NATIVE_VISIBILITY selection=%u tested=%u hidden=%u skipped=%u skipped_vertices=%u raster_calls=%u triangles=%u cell_visits=%u resets=%u geometry_bytes=%u geometry_count=%u foreign_barriers=%u query_ms=%.6f raster_ms=%.6f hidden_draw_ms=%.6f observe_keeps_all_draws=1\n",visibilityMode,v.tested,v.hidden,v.skipped,v.skippedVertices,v.rasterCalls,v.triangles,v.cells,v.resets,v.geometryBytes,v.geometryCount,v.foreignBarriers,v.queryTicks*1000.0/SYSCLOCK_ARM11,v.rasterTicks*1000.0/SYSCLOCK_ARM11,v.hiddenDrawTicks*1000.0/SYSCLOCK_ARM11);
 const char* visibilityReasons[]={"inactive","foreign","outside_world","stereo","program","projection","stencil","bias","range","depth","scissor","viewport"};
 for(unsigned i=0;i<12;++i)fprintf(f,"NATIVE_VISIBILITY_REJECT reason=%s calls=%u\n",visibilityReasons[i],v.rejected[i]);
#endif
#ifdef SRR2_OLD3DS_PARALLEL_COLLISION
 fprintf(f,"NATIVE_PARALLEL_SELECTION selection=%u\n",parallelMode);
 old3dsParallelCollisionDump(f);
#endif
#ifdef SRR2_OLD3DS_RENDER_WORKER
 fprintf(f,"NATIVE_RENDER_WORKER_SELECTION selection=%u\n",renderWorkerMode);picaRenderWorkerDump(f);
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
 fprintf(f,"NATIVE_PREPARED_BACKEND selection=%u synchronous=1 batched_mode=3 worker_mode=4 randomized_worker=5 compare_worker=6 geometry_counts_are_captures=1\n",preparedMode);
 picaPreparedBackendStats preparedSummary;picaPreparedBackendGetStats(&preparedSummary);
 fprintf(f,"NATIVE_PREPARED_WORKER available=%u core=%u initial_allowance=%u actual_allowance=%u main_fp=%u worker_fp=%u worker_draws=%u main_draws=%u jobs=%u comparisons=%u mismatches=%u\n",preparedSummary.workerAvailable,preparedSummary.workerCore,preparedSummary.initialAllowance,preparedSummary.actualAllowance,preparedSummary.mainFp,preparedSummary.workerFp,preparedSummary.workerDraws,preparedSummary.mainDraws,preparedSummary.workerJobs,preparedSummary.comparisons,preparedSummary.mismatches);
#endif
 fprintf(f,"NATIVE_PERIOD samples=%u mean_us=%.3f fps=%.6f\n",periodSamples,periodSamples?(double)periodSum*1e6/SYSCLOCK_ARM11/periodSamples:0,periodSum?(double)periodSamples*SYSCLOCK_ARM11/periodSum:0);
 fprintf(f,"NATIVE_REPLAY drains=%u timeouts=%u reload=%u reload_phase=%u\n",replayDrains,drainTimeouts,reload,reloadPhase);
 fprintf(f,"NATIVE_VERIFY calls=%u failures=%u\n",verifyAll,verifyAllFailures);
 fprintf(f,"NATIVE_COUNTERS 0=timed_scope_calls\n");
 fprintf(f,"NATIVE_SUMMARY frames=%u mean_us=%.3f max_us=%u deadline_misses=%u bookkeeping_us=%.3f complete=%u\n",samples,samples?(double)totalWork*1000000/SYSCLOCK_ARM11/samples:0,maximum,misses,(double)traceTicks*1000000/SYSCLOCK_ARM11,done && !aborted);
 for(unsigned i=0;i<Bins;++i)if(hist[i])fprintf(f,"NATIVE_HIST low_us=%u count=%u\n",i*250,hist[i]);
 for(unsigned i=recordCount>Capacity?recordCount-Capacity:0;i<recordCount;++i){const Record&r=records[i%Capacity];
 fprintf(f,"NATIVE_FRAME native_context=%u route_ms=%u pairs=%u areas=%u entities=%u frame=%u context=%u elapsed=%u real_elapsed=%u period_us=%.3f work_us=%.3f substeps=%u draws=%u vertices=%u linear=%u loading=%u x=%.6f y=%.6f z=%.6f speed=%.5f incar=%u check=%08x behavior=%08x gpu_last_ms=%.4f",r.nativeContext,r.route,r.pairs,r.areas,r.entities,r.frame,r.context,r.elapsed,r.realElapsed,(double)r.period*1e6/SYSCLOCK_ARM11,(double)r.work*1000000/SYSCLOCK_ARM11,r.substeps,r.draws,r.vertices,r.linear,r.loading,r.position[0],r.position[1],r.position[2],r.speed,r.incar,r.check,r.behavior,r.gpu);
#ifdef SRR2_OLD3DS_OCCLUSION
 fprintf(f," visibility_mode=%u visibility_tested=%u visibility_hidden=%u visibility_skipped=%u potential_draws=%u potential_vertices=%u",r.visibilityMode,r.visibilityTested,r.visibilityHidden,r.visibilitySkipped,r.draws+r.visibilitySkipped,r.vertices+r.visibilityVertices);
#endif
#ifdef SRR2_OLD3DS_PARALLEL_COLLISION
 fprintf(f," parallel_mode=%u parallel_worker_areas=%u parallel_main_areas=%u parallel_calls=%u parallel_ms=%.6f",r.parallelMode,r.parallelWorker,r.parallelMain,r.parallelCalls,r.parallelTicks*1000.0/SYSCLOCK_ARM11);
#endif
#ifdef SRR2_OLD3DS_PREPARED_BACKEND
 fprintf(f," prepared_mode=%u prepared_materials=%u prepared_uniforms=%u prepared_geometry_captures=%u prepared_skin_captures=%u prepared_views=%u prepared_lights=%u prepared_material_uniforms=%u prepared_batch_flushes=%u prepared_batched_draws=%u prepared_batch_execute_us=%.3f",r.preparedMode,r.preparedMaterials,r.preparedUniforms,r.preparedGeometry,r.preparedSkin,r.preparedViews,r.preparedLights,r.preparedMaterialUniforms,r.preparedFlushes,r.preparedBatchedDraws,r.preparedBatchTicks*1000000.0/SYSCLOCK_ARM11);
 fprintf(f," prepared_worker_draws=%u prepared_main_draws=%u prepared_jobs=%u prepared_wait_us=%.3f prepared_commit_us=%.3f prepared_comparisons=%u prepared_mismatches=%u",r.preparedWorkerDraws,r.preparedMainDraws,r.preparedJobs,r.preparedWaitTicks*1000000.0/SYSCLOCK_ARM11,r.preparedCommitTicks*1000000.0/SYSCLOCK_ARM11,r.preparedComparisons,r.preparedMismatches);
#endif
#ifdef SRR2_OLD3DS_RENDER_WORKER
 fprintf(f," worker_mode=%u worker_contexts=%u worker_main=%u worker_fences=%u worker_wait_ms=%.6f",r.workerMode,r.workerContexts,r.mainContexts,r.workerFences,r.workerWait*1000.0/SYSCLOCK_ARM11);
#endif
 for(unsigned j=0;j<Count;++j)fprintf(f," %s_us=%.3f %s_ex_us=%.3f",names[j],(double)r.stage[j]*1000000/SYSCLOCK_ARM11,names[j],(double)r.exclusive[j]*1000000/SYSCLOCK_ARM11);
 for(unsigned j=0;j<CountC;++j)fprintf(f," count_%u=%u",j,r.count[j]);fprintf(f,"\n");}fclose(f);
}
int Input(bool axis,int id,int original){
 if(!enabled||done)return original;if(!placed||warmMs<warmup*1000||done)return 0;
 // Driving-only route: retain the established first 25 seconds of inputs.
 // No exit, walking, return navigation, reset or forced mount during capture.
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
