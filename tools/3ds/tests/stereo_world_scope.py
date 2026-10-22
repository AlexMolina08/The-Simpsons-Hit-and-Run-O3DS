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
fixture=r'''
#include <cassert>
#include <vector>
#define SRR2_OLD_3DS
namespace Graphics3DS {bool stereo=false;bool Stereo(){return stereo;}}
enum {PDDI_PROJECTION_PERSPECTIVE,PDDI_PROJECTION_ORTHOGRAPHIC};
struct View {int projectionMode=PDDI_PROJECTION_PERSPECTIVE;struct {float fov=1.57f;} camera;};
struct picaContext {
 View view;struct {View* viewState;} state{&view};
 bool stereoWorldPass=false,stereoOn=false;unsigned updates=0;std::vector<bool> flushDepth;
 void SetupHardwareProjection();void FlushStreams(){flushDepth.push_back(stereoOn);}
 void BeginWorld();void EndWorld();
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
}
'''
with tempfile.TemporaryDirectory(prefix='srr-world-stereo-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture+projection+'}\n'+begin+end+main)
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: 90-degree/wide world depth, flat perspective HUD, orthographic exclusion, drain order and unchanged mono scope.')
