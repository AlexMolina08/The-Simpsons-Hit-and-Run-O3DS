#!/usr/bin/env python3
"""Exercise the production post-ending exit and timed port farewell."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
post=(root/'code/presentation/gui/ingame/guiscreencreditspostfmv.cpp').read_text()
def function(source,signature):
 start=source.index(signature);brace=source.index('{',start);end=brace+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
methods='\n'.join(function(post,s) for s in [
 'void CGuiScreenCreditsPostFMV::HandleMessage',
 'void CGuiScreenCreditsPostFMV::InitIntro()',
 'void CGuiScreenCreditsPostFMV::InitRunning()',
 'CGuiScreenCreditsPostFMV::OnScrollingDone()'])
methods=methods.replace('CGuiScreenCreditsPostFMV::OnScrollingDone()', 'void CGuiScreenCreditsPostFMV::OnScrollingDone()')
base=(root/'code/presentation/gui/frontend/guiscreenviewcredits.cpp').read_text()
a=base.index('    if(m_state == GUI_WINDOW_STATE_RUNNING && m_portThanks && message == GUI_MSG_UPDATE)')
b=base.index('\n#endif',a)
timer=base[a:b]
fixture=r'''
#include <cassert>
#include <cstdio>
#define rAssert assert
enum eGuiMessage {GUI_MSG_UPDATE,GUI_MSG_CONTROLLER_SELECT,GUI_MSG_CONTROLLER_BACK,GUI_MSG_QUIT_INGAME_FOR_RELOAD,GUI_MSG_GOTO_SCREEN};
enum {GUI_WINDOW_STATE_RUNNING,GUI_WINDOW_STATE_OUTRO,GUI_SCREEN_ID_LEVEL_END,CLEAR_WINDOW_HISTORY,EVENT_PLAY_CREDITS};
struct Parent {unsigned quits=0;virtual ~Parent(){};void HandleMessage(eGuiMessage m,unsigned level,unsigned mission){assert(m==GUI_MSG_QUIT_INGAME_FOR_RELOAD&&level==7&&mission==0);++quits;}};
struct IMemoryCardInfoLoadCallback {virtual ~IMemoryCardInfoLoadCallback()=default;};
struct Memory {bool loaded=false;unsigned requests=0;void LoadMemcardInfo(IMemoryCardInfoLoadCallback*){++requests;}bool IsMemcardInfoLoaded(){return loaded;}} memory;
auto* GetMemoryCardManager(){return &memory;}
struct Gameplay {unsigned GetCurrentLevelIndex(){return 6;}} gameplay;
auto* GetGameplayManager(){return &gameplay;}
struct Group {bool visible=false;void SetVisible(bool value){visible=value;}} group;
struct Event {void TriggerEvent(int){}} event;
auto* GetEventManager(){return &event;}
struct Sound {void DuckForInGameCredits(){}} sound;
auto* GetSoundManager(){return &sound;}
struct CGuiScreen {int m_state=GUI_WINDOW_STATE_RUNNING;unsigned baseCalls=0;void HandleMessage(eGuiMessage,unsigned=0,unsigned=0){++baseCalls;}};
struct CGuiScreenViewCredits:CGuiScreen {void HandleMessage(eGuiMessage m,unsigned a,unsigned b){CGuiScreen::HandleMessage(m,a,b);}};
struct CGuiScreenCreditsPostFMV:CGuiScreenViewCredits {
 unsigned m_exitState=0;bool m_playKKDialog=false;Group* m_creditsGroup=&group;Parent* m_pParent;
 void ResetScrolling(){}void HandleMessage(eGuiMessage,unsigned=0,unsigned=0);void InitIntro();void InitRunning();void OnScrollingDone();
};
struct Farewell:CGuiScreen {bool m_portThanks=true;unsigned m_portThanksElapsed=0,done=0;
 void OnScrollingDone(){++done;m_state=GUI_WINDOW_STATE_OUTRO;}
 void Update(eGuiMessage message,unsigned param1,unsigned param2=0){
'''+timer+r'''
 }
};
'''
main=r'''
int main(){
 Parent parent;CGuiScreenCreditsPostFMV screen;screen.m_pParent=&parent;
 screen.InitIntro();screen.InitRunning();assert(memory.requests==1&&!group.visible);
 screen.HandleMessage(GUI_MSG_CONTROLLER_SELECT);assert(screen.m_exitState==1&&!parent.quits);
 for(unsigned i=0;i<5;++i)screen.HandleMessage(GUI_MSG_UPDATE,33);
 assert(!parent.quits);memory.loaded=true;screen.HandleMessage(GUI_MSG_UPDATE,33);
 assert(parent.quits==1&&screen.m_exitState==2);
 for(unsigned i=0;i<5;++i)screen.HandleMessage(GUI_MSG_UPDATE,33);
 assert(parent.quits==1); // No repeated quit/save prompts during outro.
 Farewell farewell;farewell.Update(GUI_MSG_UPDATE,7999);assert(!farewell.done);
 farewell.Update(GUI_MSG_UPDATE,1);assert(farewell.done==1);
 farewell.Update(GUI_MSG_UPDATE,1000);assert(farewell.done==1);
 Farewell normal;normal.m_portThanks=false;normal.Update(GUI_MSG_UPDATE,10000);assert(!normal.done);
 puts("PASS: credits keep native roll; exit waits for save metadata, dispatches once; farewell returns after eight seconds; ordinary credits remain unchanged.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-ending-') as temporary:
 p=Path(temporary);(p/'test.cpp').write_text(fixture+methods+main)
 subprocess.run(['clang++','-std=c++17','-DRAD_3DS','-DSRR2_OLD_3DS','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
