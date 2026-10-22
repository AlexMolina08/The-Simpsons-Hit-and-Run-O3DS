#!/usr/bin/env python3
"""Exercise production slider calibration, camera transitions, and mono isolation."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
game = (root / 'code/main/game.cpp').read_text()
start = game.index('#ifdef RAD_3DS\n                    if(Graphics3DS::Stereo())')
hook = game[start:game.index('#endif', start) + len('#endif')]
camera_fixture = r'''
enum {CONTEXT_GAMEPLAY, CONTEXT_PAUSE, CONTEXT_DEMO, CONTEXT_SUPERSPRINT, CONTEXT_LOADING};
struct SuperCam {
    enum Type {FOLLOW_CAM, NEAR_FOLLOW_CAM, FAR_FOLLOW_CAM, CHASE_CAM, REVERSE_CAM,
        WRECKLESS_CAM, BURNOUT_CAM, SUPER_SPRINT_CAM, WALKER_CAM, BUMPER_CAM,
        FIRST_PERSON_CAM, DEFAULT_CAM, ANIMATED_CAM};
    Type type=FOLLOW_CAM;
    Type GetType() {return type;}
} active;
bool managerAllowed=true, cameraAvailable=true;
struct SuperCamCentral {SuperCam* GetActiveSuperCam(){return cameraAvailable?&active:nullptr;}} central;
struct SuperCamManager {SuperCamCentral* GetSCC(int player){assert(player==0);return &central;}} manager;
SuperCamManager* GetSuperCamManager(){assert(managerAllowed);return &manager;}
void updateCamera(int ctx, unsigned elapsed) {
''' + hook + '\n}\n'
program = r'''
#include <main/graphics3ds.h>
#include <cassert>
#include <cmath>
#include <limits>
CAMERA_FIXTURE
int main() {
    StereoDepth::State state;
    assert(state.convergence == 7.0f);
    assert(state.Shift(-1.0f) == 0 && state.Shift(0.0f) == 0);
    assert(state.Shift(0.01f) == 0 && state.Shift(1.0f) == 0.045f);
    assert(state.Shift(2.0f) == 0.045f);
    assert(state.Shift(std::numeric_limits<float>::quiet_NaN()) == 0);
    float previous = 0;
    for(unsigned step = 0; step <= 10000; ++step) {
        const float slider = float(step) / 10000.0f;
        const float value = state.Shift(slider);
        assert(std::isfinite(value) && value >= previous && value <= 0.045f);
        previous = value;
    }
    for(float slider : {.25f,.5f,.75f})
        assert(std::fabs(state.Shift(slider) / .045f - (slider-.01f)/.99f)<.000001f);
    state.Update(StereoDepth::Walking, 0); assert(state.convergence == 7);
    for(unsigned frame = 0; frame < 400; ++frame) {
        previous = state.convergence;
        state.Update(StereoDepth::Walking, 16);
        assert(state.convergence <= previous && state.convergence >= 3.5f);
    }
    assert(std::fabs(state.convergence - 3.5f) < .001f);
    StereoDepth::State slow = state, bounded = state;
    slow.Update(StereoDepth::Driving, 5000);
    bounded.Update(StereoDepth::Driving, 50);
    assert(slow.convergence == bounded.convergence && slow.convergence < 7);
    for(auto camera : {StereoDepth::Driving, StereoDepth::Walking,
                       StereoDepth::Cinematic, StereoDepth::Close}) {
        for(unsigned frame = 0; frame < 400; ++frame) state.Update(camera, 33);
        assert(std::fabs(state.convergence - StereoDepth::ScreenDistance(camera)) < .001f);
    }
    Graphics3DS::Initialize(Graphics3DS::Old3DS);
    Graphics3DS::UpdateStereoCamera(StereoDepth::Walking, 5000);
    assert(Graphics3DS::StereoConvergence() == 7); // Mono cannot change calibration.
    char app[]="test", stereo[]="graphics_stereo=1"; char* args[]={app,stereo};
    Graphics3DS::Initialize(Graphics3DS::Old3DS, 2, args);
    for(unsigned frame=0; frame<400; ++frame)
        Graphics3DS::UpdateStereoCamera(StereoDepth::Walking,16);
    assert(std::fabs(Graphics3DS::StereoConvergence()-3.5f)<.001f);
    assert(Graphics3DS::StereoShift(0)==0 && Graphics3DS::StereoShift(1)==.045f);
    Graphics3DS::Initialize(Graphics3DS::Old2DS,2,args);
    assert(!Graphics3DS::Stereo() && Graphics3DS::StereoConvergence()==7);
    managerAllowed=false;
    updateCamera(CONTEXT_GAMEPLAY,16); // Mono/2DS never touches camera singletons.
    Graphics3DS::Initialize(Graphics3DS::Old3DS,2,args);
    updateCamera(CONTEXT_LOADING,16); // Loading never touches uninitialized cameras.
    managerAllowed=true;
    for(auto type : {SuperCam::FOLLOW_CAM, SuperCam::WALKER_CAM,
                    SuperCam::ANIMATED_CAM, SuperCam::FIRST_PERSON_CAM}) {
        active.type=type;
        for(unsigned frame=0;frame<400;++frame) updateCamera(CONTEXT_GAMEPLAY,16);
        const float expected=type==SuperCam::FOLLOW_CAM?7:type==SuperCam::WALKER_CAM?3.5f:
            type==SuperCam::ANIMATED_CAM?5:2.5f;
        assert(std::fabs(Graphics3DS::StereoConvergence()-expected)<.001f);
    }
    cameraAvailable=false;
    for(unsigned frame=0;frame<400;++frame) updateCamera(CONTEXT_GAMEPLAY,16);
    assert(std::fabs(Graphics3DS::StereoConvergence()-7)<.001f);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-stereo-depth-') as directory:
    path = Path(directory)
    cpp = path / 'test.cpp'
    cpp.write_text('#define RAD_3DS\n#include <initializer_list>\n' + program.replace('CAMERA_FIXTURE', camera_fixture))
    binary = path / 'test'
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I', str(root / 'code'), str(cpp),
                    str(root / 'code/main/graphics3ds.cpp'),
                    str(root / 'code/cheats/portsettings.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], cwd=path, check=True)
print('PASS: continuous proportional slider, bounded camera transitions, reset, and mono/2DS isolation.')
