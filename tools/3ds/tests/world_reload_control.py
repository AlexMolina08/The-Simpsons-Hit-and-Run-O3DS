#!/usr/bin/env python3
"""Execute actual reload control through Pause/Mission Select and mission resets."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3];source=(root/'code/main/nativebench.cpp').read_text();a=source.index(' if(done){',source.index('unsigned Begin('));b=source.index('\n if(!benchmarkStart)',a);body=source[a:b]
code='''#include <main/worldstressplan.h>
#include <cassert>
#include <cstdio>
#include <cstring>
using u64=unsigned long long;static constexpr unsigned SYSCLOCK_ARM11=1000;
enum{CONTEXT_GAMEPLAY=9,CONTEXT_PAUSE=10,GUI_MSG_PAUSE_INGAME,GUI_MSG_QUIT_INGAME_FOR_RELOAD,GUI_MSG_CONTROLLER_SELECT};
static unsigned actualContext=CONTEXT_GAMEPLAY,currentLevel=1,currentMission=1,pauseCalls=0,dispatchCalls=0,restartCalls=0;
static bool loading=false,resetting=false,sundayDrive=true,missionScreen=false,guiAvailable=true,gameplayAvailable=true;
static unsigned engineLoadingState=0,stageCount=7;
static bool stageReady=true;
static unsigned startCalls=0,acceptCalls=0;
struct Mission{const char* GetName(){return "fixture";}unsigned GetNumStages(){return stageCount;}void* GetCurrentStage(){return stageReady?this:nullptr;}} mission;
struct Gameplay{unsigned GetCurrentLevelIndex(){return currentLevel;}unsigned GetCurrentMissionIndex(){return currentMission;}Mission* GetCurrentMission(){return &mission;}bool IsSundayDrive(){return sundayDrive;}void NextMission(){++startCalls;missionScreen=true;actualContext=CONTEXT_PAUSE;}void RestartCurrentMission(){resetting=true;++restartCalls;if(!sundayDrive){missionScreen=true;actualContext=CONTEXT_PAUSE;}}} gameplay;
auto* GetGameplayManager(){return gameplayAvailable?&gameplay:nullptr;}
struct MissionManager{enum{STATE_INVALID};unsigned GetLoadingState(){return engineLoadingState;}bool InResetState(){return resetting;}} missions;auto* GetMissionManager(){return &missions;}
struct Loading{bool IsLoading(){return loading;}} loader;auto* GetLoadingManager(){return &loader;}
struct Window{bool IsRunning(){return true;}};
struct CGuiScreenMissionLoad:Window{bool IsLoadDone(){return true;}void HandleMessage(unsigned event){assert(event==GUI_MSG_CONTROLLER_SELECT && !loading);++acceptCalls;missionScreen=false;sundayDrive=false;actualContext=CONTEXT_GAMEPLAY;}} window;
struct CGuiManagerInGame{unsigned GetCurrentScreen(){return 0;}bool IsLoadingNewMission(){return missionScreen;}Window* GetCurrentWindow(){return &window;}void HandleMessage(unsigned event,unsigned level=0,unsigned mission=0){if(event==GUI_MSG_PAUSE_INGAME){++pauseCalls;actualContext=CONTEXT_PAUSE;}else{++dispatchCalls;sundayDrive=true;currentLevel=level;currentMission=mission;actualContext=CONTEXT_GAMEPLAY;}}} gui;
struct GuiSystem{CGuiManagerInGame* GetInGameManager(){return guiAvailable?&gui:nullptr;}} systemGui;auto* GetGuiSystem(){return &systemGui;}
namespace WorldResidency { struct LevelStatus{unsigned expected,bound,bad,wanted;};LevelStatus Status(unsigned){return {733,733,0,3};} }
struct Wrappers {void DumpIntegrity(FILE*){}} wrappers;Wrappers* GetAllWrappers(){return &wrappers;}
void Old3dsMemoryDump(FILE*){}
static WorldStress::Plan reloadPlan;
static WorldStress::FrameStats storage[65];static WorldStress::FrameStats* reloadStats=storage;
static unsigned reloadPhase=0,reloadMs=0,reloadDispatches=0,reloadObservedLevels=0,portraitLevel=2;
static bool reloadPauseRequested=false,done=true,reload=true,aborted=false,reloadSampling=false,reloadEntered[65]={},reloadReset[65]={},reloadTeleported[65]={};
static bool reloadInteriorDone[65]={};namespace WorldInteriorStress {int Step(unsigned,unsigned,int){return 1;}}
unsigned linearSpaceFree(){return 3000000;}
static bool reloadMissionRequested[65]={};static unsigned reloadReportMs=0;
static WorldStress::PhaseWatchdog reloadPhaseWatchdog;
static unsigned teleportCalls=0;bool ReloadTeleport(unsigned){++teleportCalls;return true;}
static u64 benchmarkStart=1,ticks=1;
unsigned Step(unsigned elapsed,int context){u64 now=ticks;reloadSampling=false;
'''+body+'''return elapsed;}
int main(){
 assert(reloadPlan.Parse("4:7,1:2,4:2"));reloadPlan.dwellMs=3000;reloadPlan.resetMission=true;reloadPlan.teleport=true;reloadPlan.interior=true;reloadPlan.activeMission=true;
 guiAvailable=false;reloadReportMs=30000;Step(1000,CONTEXT_PAUSE);assert(!pauseCalls && !aborted);
 guiAvailable=false;gameplayAvailable=false;reloadReportMs=30000;Step(1000,CONTEXT_PAUSE);assert(!pauseCalls && !aborted);guiAvailable=gameplayAvailable=true;
 loading=true;engineLoadingState=1;Step(1000,actualContext);assert(!pauseCalls);loading=false;engineLoadingState=0;
 engineLoadingState=2;Step(1000,actualContext);assert(!pauseCalls);engineLoadingState=0;
 stageCount=0;Step(1000,actualContext);assert(!pauseCalls);stageCount=7;
 stageReady=false;Step(1000,actualContext);assert(!pauseCalls);stageReady=true;
 Step(1000,actualContext);assert(actualContext==CONTEXT_PAUSE && !dispatchCalls);
 Step(1000,actualContext);assert(currentLevel==3 && currentMission==6 && reloadPhase==1);
 currentMission=0;Step(1000,actualContext);assert(!reloadEntered[1]);currentMission=6;
 bool sampledStreaming=false;
 for(unsigned frame=0;frame<100 && reloadPhase<4;++frame){
  if(!sampledStreaming && reloadPhase==1 && reloadEntered[1] && reloadTeleported[1] && reloadInteriorDone[1] && !reloadReset[1]){
   loading=true;unsigned before=reloadMs;Step(100,actualContext);assert(reloadMs==before+100);loading=false;sampledStreaming=true;
  }
  ticks+=1000;Step(1000,actualContext);
  if(resetting){unsigned before=reloadMs;Step(1000,actualContext);assert(reloadMs==before);resetting=false;}
 }
 assert(sampledStreaming && reloadPhase==4 && !aborted && dispatchCalls==3 && pauseCalls==3 && reloadDispatches==3 && restartCalls==3 && teleportCalls==3 && startCalls==3 && acceptCalls==6);
 for(unsigned i=1;i<=3;++i)assert(reloadEntered[i] && reloadStats[i].resets==1 && reloadStats[i].teleports==1 && reloadStats[i].interiors==1 && reloadStats[i].missionActive==1 && reloadStats[i].residentExpected==733 && reloadStats[i].residentBound==733 && !reloadStats[i].residentBad);
 reloadPhase=0;reloadPauseRequested=false;loading=true;ticks=1000000;Step(1000,CONTEXT_GAMEPLAY);assert(aborted && reloadPhase==4);
}
'''
with tempfile.TemporaryDirectory(prefix='world-reload-control-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual pause/reload controller, mission identity, loading/reset barriers, one restart per destination and watchdog exit.')
