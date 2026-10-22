#!/usr/bin/env python3
"""Exercise production world/HUD scope with wide-angle and mono projections."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
projection=s[s.index('    bool depthView ='):s.index('\n    ApplyProjection(&projection);',s.index('    bool depthView ='))]
def prefix(name,ending):
 start=s.index('void picaContext::'+name+'(void)')
 return s[start:s.index(ending,start)]+'}\n'
begin=prefix('BeginWorld','    if(display->BeginWorldTarget())')
end=prefix('EndWorld','    if(!display->IsWorldTarget()) return;')
objects=s[s.index('bool picaContext::BeginStereoObject('):s.index('void picaContext::BeginWorld(void)')]
menu=(root/'libs/scrooby/src/FePure3dObject.cpp').read_text()
start=menu.index('    const bool stereoModel =')
selector=menu[start:menu.index(';',start)+1]
fixture=r'''
#include <cassert>
#include <vector>
#include <cstring>
#define SRR2_OLD_3DS
namespace Graphics3DS {bool stereo=false;bool Stereo(){return stereo;}}
enum {PDDI_PROJECTION_PERSPECTIVE,PDDI_PROJECTION_ORTHOGRAPHIC};
struct View {int projectionMode=PDDI_PROJECTION_PERSPECTIVE;struct {float fov=1.57f;} camera;};
struct picaContext {
 View view;struct {View* viewState;} state{&view};
 bool stereoWorldPass=false,stereoObjectPass=false,stereoOn=false,bottomPass=false;
 float stereoObjectDistance=0;unsigned updates=0;std::vector<bool> flushDepth;
 void SetupHardwareProjection();void FlushStreams(){flushDepth.push_back(stereoOn);}
 void BeginWorld();void EndWorld();
 bool BeginStereoObject(float);void EndStereoObject();
};
void picaContext::SetupHardwareProjection(){++updates;
'''
main=r'''
int main(){
 picaContext c;
 c.BeginWorld();c.EndWorld();assert(c.updates==0 && c.flushDepth.empty());
 c.SetupHardwareProjection();assert(!c.stereoOn); // Existing mono heuristic remains unchanged.
 c.view.camera.fov=1.0f;c.SetupHardwareProjection();assert(c.stereoOn);
 Graphics3DS::stereo=true;c.SetupHardwareProjection();assert(!c.stereoOn); // Perspective HUD stays flat.
 c.view.camera.fov=1.5707963f;c.BeginWorld();assert(c.stereoOn);
 assert(c.flushDepth.size()==1 && !c.flushDepth.back());
 c.view.camera.fov=2.0f;c.SetupHardwareProjection();assert(c.stereoOn); // Wide world view retains depth.
 c.BeginWorld();assert(c.flushDepth.size()==1); // Repeated begin does not flush twice.
 c.view.projectionMode=PDDI_PROJECTION_ORTHOGRAPHIC;c.SetupHardwareProjection();assert(!c.stereoOn);
 c.view.projectionMode=PDDI_PROJECTION_PERSPECTIVE;c.SetupHardwareProjection();assert(c.stereoOn);
 c.EndWorld();assert(!c.stereoOn && !c.stereoWorldPass);
 assert(c.flushDepth.size()==2 && c.flushDepth.back()); // Last world geometry drains with depth still enabled.
 c.view.camera.fov=1.0f;c.SetupHardwareProjection();assert(!c.stereoOn);
 c.EndWorld();assert(c.flushDepth.size()==2);
 assert(c.BeginStereoObject(4.5f));assert(c.stereoOn && c.stereoObjectDistance==4.5f);
 assert(!c.BeginStereoObject(.66f)); // Nested objects cannot overwrite the scene plane.
 c.EndStereoObject();assert(!c.stereoOn && c.stereoObjectDistance==0);
 assert(c.flushDepth.size()==4 && !c.flushDepth[2] && c.flushDepth[3]);
 c.EndStereoObject();assert(c.flushDepth.size()==4);
 c.bottomPass=true;assert(!c.BeginStereoObject(.66f));
 c.BeginWorld();assert(!c.stereoOn); // Lower screen remains flat, including perspective widgets.
 assert(!c.BeginStereoObject(.66f));c.EndWorld();c.bottomPass=false;
 c.BeginWorld();assert(!c.BeginStereoObject(.66f));c.EndWorld();
 Graphics3DS::stereo=false;assert(!c.BeginStereoObject(4.5f));
 Graphics3DS::stereo=true;activeContext=&c;
 Camera room{37},other{99};
 assert(RenderMenu(&room)); // Release builds retain the UID, not the camera's name string.
 assert(!RenderMenu(&other) && !RenderMenu(nullptr));
}
'''
menu_fixture=r'''
struct tEntity {static unsigned MakeUID(const char* name){assert(!strcmp(name,"FE_Camera"));return 37;}};
struct Camera {unsigned uid;unsigned GetUID(){return uid;}const char* GetName(){return "NO_NAME_TEXT";}};
picaContext* activeContext=nullptr;
bool picaStereoObjectBegin(float distance){return activeContext->BeginStereoObject(distance);}
bool RenderMenu(Camera* m_Camera){
'''+selector+r'''
 if(stereoModel)activeContext->EndStereoObject();return stereoModel;
}
'''
with tempfile.TemporaryDirectory(prefix='srr-world-stereo-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture+projection+'}\n'+begin+end+objects+menu_fixture+main)
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: world/menu/artwork depth scopes, nested-scope rejection, flat HUD/lower screen, drain order and unchanged mono scope.')
