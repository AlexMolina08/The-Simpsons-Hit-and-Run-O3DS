#!/usr/bin/env python3
"""Run production benchmark automation against a small engine fixture."""
from pathlib import Path
import subprocess,tempfile,json
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory)
 (p/'3ds.h').write_text('''#pragma once
using u64=unsigned long long;const unsigned SYSCLOCK_ARM11=1000;
static u64 ticks=0;inline u64 svcGetSystemTick(){return ++ticks;}
inline void svcSleepThread(long long){++ticks;}inline unsigned linearSpaceFree(){return 123456;}
''')
 (p/'fixture.h').write_text('''#pragma once
#include <3ds.h>
namespace rmt {struct Vector{float x=0,y=0,z=0;};}
const int CONTEXT_GAMEPLAY=1;const int O3_CATEGORY_COUNT=2;
struct E{unsigned calls=0;};E g_old3dsEntities[2];
unsigned g_old3dsDrawCalls=0,g_old3dsDrawVerts=0,g_old3dsPhysicsSubsteps=0,g_old3dsCollisionPairs=0,g_old3dsPhysicsAreas=0;
struct Character{};struct Vehicle{unsigned resets=0;void Reset(bool,bool){++resets;}} vehicle;
struct Avatar{Character c;bool incar=false;Character* GetCharacter(){return &c;}void SetCameraTargetToVehicle(bool){}void GetPosition(rmt::Vector&){}void GetVelocity(rmt::Vector&){}float GetSpeedMps(){return 0;}bool IsInCar(){return incar;}} avatar;
struct Avatars{Avatar* GetAvatarForPlayer(int){return &avatar;}void PutCharacterInCar(Character*,Vehicle*){avatar.incar=true;}} avatars;
inline Avatars* GetAvatarManager(){return &avatars;}
struct Loading{bool IsLoading(){return false;}unsigned GetNumCurrentRequests(){return 0;}} loading;
inline Loading* GetLoadingManager(){return &loading;}
struct Gameplay{Vehicle* GetCurrentVehicle(){return &vehicle;}int GetCurrentLevelIndex(){return 1;}} gameplay;
inline Gameplay* GetGameplayManager(){return &gameplay;}
namespace RenderEnums{enum{L1,L2,M2};}
struct SuperCam{enum{FAR_FOLLOW_CAM};};struct SuperCamCentral{enum{CUT=1,FORCE=2};void SelectSuperCam(int,int){}} camera;
struct Cameras{SuperCamCentral* GetSCC(int){return &camera;}} cameras;inline Cameras* GetSuperCamManager(){return &cameras;}
const int GUI_MSG_QUIT_INGAME_FOR_RELOAD=0;struct Gui{Gui* GetInGameManager(){return this;}void HandleMessage(int,int,int){}} gui;inline Gui* GetGuiSystem(){return &gui;}
namespace CommandLineOptions{static unsigned calls=0;inline void HandleOption(char*){++calls;}}
inline void radFileService(){}namespace p3d{struct Loader{void SwitchTask(){}void TriggerCallbacks(){}} loader;static Loader* loadManager=&loader;}
inline float C3D_GetDrawingTime(){return 0;}
enum{SDL_CONTROLLER_AXIS_LEFTY,SDL_CONTROLLER_AXIS_LEFTX,SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_X};
''')
 for name in ['main/commandlineoptions.h','contexts/context.h','loading/loadingmanager.h','worldsim/avatarmanager.h','worldsim/character/character.h','worldsim/redbrick/vehicle.h','mission/gameplaymanager.h','main/old3dsdiag.h','citro3d.h','radfile.hpp','p3d/loadmanager.hpp','SDL.h','camera/supercammanager.h','camera/supercamcentral.h','camera/supercam.h','presentation/gui/guisystem.h','presentation/gui/ingame/guimanageringame.h']:
  q=p/name;q.parent.mkdir(parents=True,exist_ok=True);q.write_text('#include <fixture.h>\n')
 (p/'test.cpp').write_text('''#include <cassert>
#include <cstdlib>
static bool failAllocation=false;
static void* benchmarkAlloc(size_t n,size_t size){return failAllocation?nullptr:std::calloc(n,size);}
#define calloc benchmarkAlloc
#include <main/nativebench.cpp>
int main(int argc,char**argv){
 failAllocation=argc>1 && argv[1][0]=='f';
 if(failAllocation){NativeBench::Init();assert(NativeBench::End(20,CONTEXT_GAMEPLAY));assert(vehicle.resets==0);return 0;}
 if(argc>1){FILE*f=fopen("native-automation.txt","w");fprintf(f,"mode=replay\\nseconds=5\\nwarmup=3\\nstep_ms=50\\ndetail=0\\n");fclose(f);}
 NativeBench::Init(argc,argv);assert(CommandLineOptions::calls==3);assert(NativeBench::Seed(99)==231171);
 unsigned dt=NativeBench::Begin(123,0);assert(dt==(argc>1?50:123));assert(!NativeBench::collecting);
 bool finish=false;for(unsigned i=0;i<1000 && !finish;++i){
  dt=NativeBench::Begin(200,CONTEXT_GAMEPLAY);assert(dt==(argc>1?50:200));
  ++g_old3dsDrawCalls;g_old3dsDrawVerts+=3;++g_old3dsPhysicsSubsteps;
  finish=NativeBench::End(20,CONTEXT_GAMEPLAY);
 }
 assert(finish && vehicle.resets==1 && avatar.incar);NativeBench::Dump();
 assert(NativeBench::Input(false,SDL_CONTROLLER_BUTTON_A,7)==7);
 NativeBench::done=false;
 for(unsigned t=0;t<25000;t+=50){
  NativeBench::routeMs=t;
  assert(!NativeBench::Input(false,SDL_CONTROLLER_BUTTON_X,0));
  assert(!NativeBench::Input(false,SDL_CONTROLLER_BUTTON_Y,0));
 }

}
''')
 subprocess.run(['c++','-std=c++17','-DSRR2_OLD3DS_NATIVE_BENCH','-DSRR2_OLD3DS_BUILD_ID="automation-test"','-fsanitize=address,undefined','-I'+str(p),'-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),'fail'],cwd=p,check=True)
 for args,mode in [([], 'realtime'),(['replay'],'replay'),(['replay','bench_mode=replay','bench_resident=1','bench_step=50'],'replay')]:
  subprocess.run([str(p/'test')]+args,cwd=p,check=True)
  result=subprocess.run(['python3',str(root/'tools/3ds/summarize_native.py'),str(p/'native-automation-test.log'),'--source','emulator'],check=True,capture_output=True,text=True)
  report=json.loads(result.stdout);assert report['header']['mode']==mode
  if len(args)>1:assert 'resident=1' in (p/'native-automation-test.log').read_text()
  assert report['summary']['complete']=='1' and not report['route_health']['coverage_pass']
  assert report['motion']['moving_frames']==0 # A completed input script is not a successful route.
print('PASS: production startup, seed, one mount/reset, real elapsed vs replay, automatic exit, allocation-failure exit, and rejected stationary route')
