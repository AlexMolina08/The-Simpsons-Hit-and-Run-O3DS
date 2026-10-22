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
flag_start=s.index('#ifdef SRR2_3DS_STEREO_GPU_SKIN\n        if(skinProgramBound && Graphics3DS::Stereo())')
skin_flag=s[flag_start:s.index('#endif',flag_start)+len('#endif')]
menu=(root/'libs/scrooby/src/FePure3dObject.cpp').read_text()
assert 'picaStereoObjectBegin(' not in menu, 'Frontend close-up cameras must remain flat'
fixture=r'''
#include <cassert>
#include <vector>
#include <cstring>
#define SRR2_OLD_3DS
#define SRR2_3DS_STEREO_GPU_SKIN
namespace Graphics3DS {bool stereo=false;bool Stereo(){return stereo;}}
enum {GPU_VERTEX_SHADER};std::vector<bool> skinDepth;
void C3D_BoolUnifSet(int shader,int slot,bool value){assert(shader==GPU_VERTEX_SHADER && slot==0x6b);skinDepth.push_back(value);}
enum {PDDI_PROJECTION_PERSPECTIVE,PDDI_PROJECTION_ORTHOGRAPHIC};
struct View {int projectionMode=PDDI_PROJECTION_PERSPECTIVE;struct {float fov=1.57f;} camera;};
struct picaContext {
 View view;struct {View* viewState;} state{&view};
 bool stereoWorldPass=false,stereoObjectPass=false,stereoOn=false,bottomPass=false;
 bool skinProgramBound=true;void UploadStereoFlag();
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
 Graphics3DS::stereo=true;
 // Frontend zooms cannot acquire depth from the legacy narrow-FOV heuristic.
 for(float fov:{.3f,1.0f,1.5707963f,2.4f}){
  c.view.camera.fov=fov;c.SetupHardwareProjection();assert(!c.stereoOn);
  c.UploadStereoFlag();assert(!skinDepth.back());
 }
 c.BeginWorld();c.UploadStereoFlag();assert(skinDepth.back());
 c.EndWorld();c.UploadStereoFlag();assert(!skinDepth.back());
 c.BeginStereoObject(4.5f);c.UploadStereoFlag();assert(skinDepth.back());
 c.EndStereoObject();c.UploadStereoFlag();assert(!skinDepth.back());
 const unsigned writes=skinDepth.size();c.skinProgramBound=false;c.UploadStereoFlag();
 c.skinProgramBound=true;Graphics3DS::stereo=false;c.UploadStereoFlag();assert(skinDepth.size()==writes);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-world-stereo-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture+projection+'}\n'+begin+end+objects+'void picaContext::UploadStereoFlag(){\n'+skin_flag+'\n}\n'+main)
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: world/artwork depth scopes, flat frontend zooms/HUD/lower screen, nested-scope rejection, drain order and unchanged mono scope.')
