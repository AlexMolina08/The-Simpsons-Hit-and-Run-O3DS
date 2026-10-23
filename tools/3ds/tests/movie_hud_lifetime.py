#!/usr/bin/env python3
"""Exercise production movie callbacks, memory fallback and resource release order."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/presentation/presentation.cpp').read_text()
def block(signature):
 a=source.index(signature);b=source.index('{',a);end=b+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[a:end]
methods='\n'.join(block(s) for s in ['void PresentationManager::OnPresentationEventBegin(',
 'void PresentationManager::OnPresentationEventEnd(', 'void PresentationManager::OnProcessRequestsComplete(', 'void PresentationManager::ClearQueue()'])
finished=block('        if( finished )')
program=r'''
#include <cassert>
#include <cstdio>
#include <vector>
#define rAssert assert
enum {CONTEXT_GAMEPLAY,GMA_LEVEL_HUD,GUI_MSG_RELEASE_INGAME,GUI_MSG_INIT_INGAME,GUI_MSG_RUN_INGAME,BLACK_TRANSPARENT,BLACK};
namespace RenderEnums {enum {PresentationSlot};}
struct Context {virtual ~Context()=default;};
struct GameplayContext:Context {std::vector<bool> changes;void PauseAllButPresentation(bool p){changes.push_back(p);}} context;
struct Flow {Context* GetContext(int){return &context;}int GetCurrentContext(){return CONTEXT_GAMEPLAY;}} flow;
auto* GetGameFlow(){return &flow;}
unsigned freeLinear=3*1024*1024,usedHeap=14*1024*1024;
unsigned linearSpaceFree(){return freeLinear;}
struct HeapInfo {unsigned uordblks;};HeapInfo mallinfo(){return {usedHeap};}
struct Screen {float alpha=0;void SetAlpha(float a){alpha=a;}} screen;
struct CGuiScreenHud {Screen* GetScroobyScreen(){return &screen;}} hud;
auto* GetCurrentHud(){return &hud;}
struct CGuiWindow {enum {GUI_SCREEN_ID_HUD=33};};
struct CGuiManagerInGame {int id=33;int GetCurrentScreen(){return id;}bool IsHudScreen(int i){return i==33;}} manager;
struct Gui {unsigned released=0,initialized=0,runs=0;CGuiManagerInGame* GetInGameManager(){return &manager;}void HandleMessage(int m){if(m==GUI_MSG_RELEASE_INGAME)++released;if(m==GUI_MSG_INIT_INGAME)++initialized;if(m==GUI_MSG_RUN_INGAME)++runs;}} gui;
auto* GetGuiSystem(){return &gui;}
struct Heap {void PushHeap(int){}void PopHeap(int){}} heap;
auto* HeapMgr(){return &heap;}
struct PresentationOverlay {int frames=-1;bool removed=false;void SetStart(int){}void SetEnd(int){}void SetDuration(float){}void SetFrames(int n){frames=n;}void SetRemoveOnComplete(bool b){removed=b;}} overlay;
struct RenderLayer {void RemoveGuts(PresentationOverlay*){}void AddGuts(PresentationOverlay*){}} layer;
struct RenderManager {RenderLayer* mpLayer(int){return &layer;}void ThawFromPresentation(){}} renderer;
auto* GetRenderManager(){return &renderer;}
struct Gameplay {void ResetIdleTime(){}} gameplay;
auto* GetGameplayManager(){return &gameplay;}
struct Callback;bool movieFreed=false;unsigned stopCalls=0;
struct PresentationEvent {Callback* pCallback=nullptr;void* player=nullptr;void* GetPlayer(){return player;}void Stop(){movieFreed=true;++stopCalls;}} event;
struct Callback {virtual ~Callback()=default;virtual void OnPresentationEventBegin(PresentationEvent*){}virtual void OnPresentationEventEnd(PresentationEvent*){}};
struct Drawable {void SetPlayer(void*){}} drawable;
struct PresentationManager {
 Drawable* mpPlayerDrawable=&drawable;
 bool m_retainedMovieHud=false;PresentationOverlay* mOverlay=&overlay;
 Callback* mpPlayCallback=nullptr;PresentationEvent* mpCurrent=&event;void* mpFMVPlayer=this;
 void OnPresentationEventBegin(PresentationEvent*);void OnPresentationEventEnd(PresentationEvent*);
 void OnProcessRequestsComplete(void*);void ClearQueue();PresentationEvent* GetFirst(){return nullptr;}void ReturnToPool(PresentationEvent*){}
 void Finish(){bool finished=true;
'''+finished+r'''
 }
};
struct Loading {bool pending=false;PresentationManager* callback=nullptr;
 void AddCallback(PresentationManager* p){assert(overlay.frames==-1);if(pending)callback=p;else p->OnProcessRequestsComplete(nullptr);}
 void Complete(){assert(callback);pending=false;auto*p=callback;callback=nullptr;p->OnProcessRequestsComplete(nullptr);}
} loading;
auto* GetLoadingManager(){return &loading;}
'''+methods+r'''
struct Adapter:Callback {PresentationManager* owner;void OnPresentationEventEnd(PresentationEvent*p)override{owner->OnPresentationEventEnd(p);}} adapter;
struct Client:Callback {unsigned count=0;void OnPresentationEventEnd(PresentationEvent*)override{assert(movieFreed);++count;}} client;
int main(){
 for(unsigned scenario=0;scenario<6;++scenario){
  gui={};overlay={};context.changes.clear();screen.alpha=0;client.count=0;stopCalls=0;movieFreed=false;
  freeLinear=scenario==2?2*1024*1024-1:3*1024*1024;
  usedHeap=scenario==3?24*1024*1024+1:14*1024*1024;manager.id=scenario==4?1:33;
  PresentationManager presentation;presentation.mpPlayCallback=&client;adapter.owner=&presentation;
  event.player=presentation.mpFMVPlayer;event.pCallback=&adapter;
  loading.pending=scenario!=0&&scenario!=5;loading.callback=nullptr;
  presentation.OnPresentationEventBegin(&event);
  const bool retained=scenario<2||scenario==5;
  assert(presentation.m_retainedMovieHud==retained&&gui.released==!retained);
  if(scenario==5)presentation.ClearQueue();else presentation.Finish();assert(movieFreed&&stopCalls==1&&!presentation.mpCurrent);
  if(loading.callback)loading.Complete();
  assert(client.count==1&&!presentation.mpPlayCallback&&!presentation.m_retainedMovieHud);
  assert(gui.initialized==!retained&&gui.runs==!retained);
  if(retained)assert(screen.alpha==1);
  assert(context.changes==std::vector<bool>({true,false}));
  assert(overlay.frames==10&&overlay.removed);
 }
 puts("PASS: retained HUD returns without reload; pending loads remain a barrier; decoder stops once before return; low linear/heap memory and non-HUD screens keep legacy path.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-movie-hud-') as temporary:
 p=Path(temporary);(p/'test.cpp').write_text(program)
 subprocess.run(['clang++','-std=c++17','-DSRR2_OLD_3DS','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
