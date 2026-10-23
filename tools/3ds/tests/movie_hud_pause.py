#!/usr/bin/env python3
"""Exercise the production GUI update gate during retained movie playback."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/contexts/gameplay/gameplaycontext.cpp').read_text()
a=source.index('    #ifdef SRR2_OLD_3DS\n    // Retained movie HUD')
b=source.index('\n#ifdef DEBUGWATCH',a)
program=r'''
#include <cassert>
#include <cstdio>
#define SRR2_OLD_3DS
#define O3_RECUR(value)
unsigned g_scroobySimulationTime=0,queries=0,updates=0;
struct Presentation {bool retained=false;bool RetainsMovieHud(){++queries;return retained;}} presentation;
auto* GetPresentationManager(){return &presentation;}
struct PlayingContext {void OnUpdate(unsigned elapsed){++updates;g_scroobySimulationTime=elapsed;}};
struct GameplayContext:PlayingContext {bool m_PausedAllButPresentation=false;
 void UpdateGUI(unsigned elapsedTime){
'''+source[a:b]+r'''
 }
};
int main(){
 GameplayContext gameplay;
 for(unsigned i=0;i<10000;++i)gameplay.UpdateGUI(17);
 assert(updates==10000&&queries==0&&g_scroobySimulationTime==17);
 gameplay.m_PausedAllButPresentation=true;gameplay.UpdateGUI(33);
 assert(updates==10001&&queries==1&&g_scroobySimulationTime==33);
 presentation.retained=true;gameplay.UpdateGUI(34);
 assert(updates==10001&&queries==2&&g_scroobySimulationTime==0);
 presentation.retained=false;gameplay.UpdateGUI(16);
 assert(updates==10002&&queries==3&&g_scroobySimulationTime==16);
 gameplay.m_PausedAllButPresentation=false;gameplay.UpdateGUI(17);
 assert(updates==10003&&queries==3);
 puts("PASS: movie HUD pauses timers, touch, animation and GUI rendering; legacy paths stay active; normal gameplay avoids movie-state queries.");
}
'''
renderer=(root/'code/render/RenderManager/RenderManager.cpp').read_text()
a=renderer.index('#ifdef RAD_3DS\n#ifdef SRR2_OLD_3DS\n    const bool moviePlaying')
b=renderer.index('    END_PROFILE( "Rendering" );',a)
render_fixture=r'''
#define RAD_3DS
#define BEGIN_PROFILE(value)
#define END_PROFILE(value)
unsigned nativeBefore=0,nativeAfter=0,worldBegin=0,worldEnd=0,bottomClears=0,bottomEnds=0;
bool picaBottomScreenInterfaceBegin(){++bottomClears;return true;}void picaBottomScreenEnd(){++bottomEnds;}
struct CHudMap {static void BeforeRenderLayers3DS(){++nativeBefore;}static void AfterRenderLayers3DS(void*){++nativeAfter;}};
void picaWorldBegin(){++worldBegin;}void picaWorldEnd(){++worldEnd;}
namespace RenderEnums {enum {GUI,PresentationSlot,numLayers=3};}
struct Layer {unsigned draws=0;bool IsRenderReady(){return true;}void Render(){++draws;}void* pView(unsigned){return nullptr;}};
Layer layers[3];Layer* mpRenderLayers[]={layers,layers+1,layers+2};
void RenderFrame(){
'''+renderer[a:b]+'}\n'
program=program.replace('struct Presentation {bool retained=false;', 'struct FMV {bool playing=false;bool IsPlaying(){return playing;}} fmv;\nstruct Presentation {FMV* GetFMVPlayer(){return &fmv;}bool retained=false;')
program=program.replace('int main(){',render_fixture+'\nint main(){')
a=' puts("PASS: hidden retained HUD'
b=r'''
 for(bool movie:{false,true})for(bool retained:{false,true}){
  fmv.playing=movie;presentation.retained=retained;queries=0;
  nativeBefore=nativeAfter=worldBegin=worldEnd=bottomClears=bottomEnds=0;for(auto& layer:layers)layer.draws=0;
  RenderFrame();const bool dormant=movie&&retained;
  assert(nativeBefore==!dormant&&nativeAfter==!dormant&&layers[RenderEnums::GUI].draws==!dormant);
  assert(layers[RenderEnums::PresentationSlot].draws==1&&layers[2].draws==1);
  assert(worldBegin==!movie&&worldEnd==1&&queries==movie);
  assert(bottomClears==dormant&&bottomEnds==dormant);
 }
 puts("PASS: hidden retained HUD'''
program=program.replace(a,b)
program=program.replace('#include <cstdio>', '#include <cstdio>\n#include <initializer_list>')
with tempfile.TemporaryDirectory(prefix='srr-hud-pause-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(program)
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
