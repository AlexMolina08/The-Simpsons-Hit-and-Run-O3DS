#!/usr/bin/env python3
"""Exercise the production letterbox outro across mission restart/teardown."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root/'code/presentation/gui/ingame/guiscreenletterbox.cpp').read_text()
begin = source.index('void CGuiScreenLetterBox::InitOutro()')
brace = source.index('{', begin)
end, depth = brace+1, 1
while depth:
    depth += (source[end] == '{')-(source[end] == '}')
    end += 1
method = source[begin:end]
program = r'''
#include <cassert>
#include <cstdio>
#define rAssert assert
enum {EVENT_CONVERSATION_DONE};
struct Transition {
    unsigned activated=0, deactivated=0;
    void Activate(){++activated;} void Deactivate(){++deactivated;}
    void SetNextTransition(void* p){assert(!p);}
} g_TopIn,g_BottomIn,g_TopOut,g_BottomOut,g_TopClose,g_BottomClose;
bool g_SurpressNextSkipButton=true;
struct MissionObjective {
    bool patty=false,race=false;
    bool IsPattyAndSelmaDialog(){return patty;} bool IsRaceDialog(){return race;}
} objective;
struct MissionStage {MissionObjective* obj=&objective;auto* GetObjective(){return obj;}} stage;
struct Mission {
    int current=0;MissionStage* active=&stage;
    int GetNumStages(){return 3;} int GetCurrentStageIndex(){return current;}
    MissionStage* GetCurrentStage(){return current>=0&&current<3?active:nullptr;}
} mission;
struct Gameplay {
    bool sunday=false;Mission* active=&mission;
    bool IsSundayDrive(){return sunday;} Mission* GetCurrentMission(){return active;}
} gameplay;
auto* GetGameplayManager(){return &gameplay;}
struct Button {bool visible=true;void SetVisible(bool value){visible=value;}} button;
struct Events {unsigned removed=0;void RemoveListener(void*,int){++removed;}} events;
auto* GetEventManager(){return &events;}
struct CGuiScreenLetterBox {
    bool m_OutroPending=false,m_forceOpen=false;Button* m_skipButton=&button;
    unsigned m_elapsedTime=999,m_numTransitionsPending=0;
    void InitOutro();
};
'''+method+r'''
int main(){
    unsigned cases=0;
    for(unsigned scenario=0;scenario<6;++scenario)
    for(unsigned flags=0;flags<16;++flags){
        g_TopIn={};g_BottomIn={};g_TopOut={};g_BottomOut={};g_TopClose={};g_BottomClose={};
        objective={bool(flags&2),bool(flags&4)};stage={};mission={};gameplay={};button={};events={};
        gameplay.sunday=flags&1;
        if(scenario==1)mission.current=2;
        if(scenario==2)mission.current=-1;
        if(scenario==3)mission.current=3;
        if(scenario==4)stage.obj=nullptr;
        if(scenario==5)gameplay.active=nullptr;
        CGuiScreenLetterBox screen;screen.m_forceOpen=flags&8;g_SurpressNextSkipButton=true;
        screen.InitOutro();
        const bool hasStage=scenario<2||scenario==4;
        const bool hasObjective=scenario<2;
        const bool close=!(flags&8)&&((hasStage&&gameplay.sunday&&mission.current==2)
                          ||(hasObjective&&(objective.patty||objective.race)));
        assert(g_TopClose.activated==close&&g_BottomClose.activated==close);
        assert(g_TopOut.activated==!close&&g_BottomOut.activated==!close);
        assert(!button.visible&&!screen.m_elapsedTime&&screen.m_numTransitionsPending==1);
        assert(screen.m_OutroPending&&!screen.m_forceOpen&&!g_SurpressNextSkipButton);
        assert(g_TopIn.deactivated==1&&g_BottomIn.deactivated==1&&events.removed==1);
        screen.InitOutro();assert(screen.m_numTransitionsPending==1&&events.removed==1);
        ++cases;
    }
    printf("PASS: %u production letterbox cases preserve valid dialogue transitions and survive cleared stages/missions.\n",cases);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-letterbox-lifetime-') as temporary:
    path = Path(temporary)
    (path/'test.cpp').write_text(program)
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(path/'test.cpp'), '-o', str(path/'test')], check=True)
    subprocess.run([str(path/'test')], check=True)
