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
namespace rmt {struct Vector {float x=0,y=0,z=0;};}
enum {CONTEXT_GAMEPLAY, CONTEXT_PAUSE, CONTEXT_DEMO, CONTEXT_SUPERSPRINT, CONTEXT_LOADING};
bool managerAllowed=true, cameraAvailable=true, avatarAvailable=true;
struct tPointCamera {
    float nearPlane=1;
    float GetNearPlane(){return nearPlane;}
    void WorldToCamera(const rmt::Vector& position,rmt::Vector* out){*out=position;}
} viewCamera;
struct Avatar {
    rmt::Vector position{10,2,5};
    void* GetCharacter(){return this;}
    void GetPosition(rmt::Vector& out){out=position;}
} avatar;
struct AvatarManager {Avatar* GetAvatarForPlayer(int player){assert(player==0);return avatarAvailable?&avatar:nullptr;}} avatars;
AvatarManager* GetAvatarManager(){assert(managerAllowed);return &avatars;}
struct SuperCam {
    enum Type {FOLLOW_CAM, NEAR_FOLLOW_CAM, FAR_FOLLOW_CAM, CHASE_CAM, REVERSE_CAM,
        WRECKLESS_CAM, BURNOUT_CAM, SUPER_SPRINT_CAM, WALKER_CAM, BUMPER_CAM,
        FIRST_PERSON_CAM, DEFAULT_CAM, ANIMATED_CAM};
    Type type=FOLLOW_CAM;
    Type GetType(){return type;}
    tPointCamera* GetCamera(){return &viewCamera;}
    // Look-ahead and virtual transition targets must never anchor stereo.
    void GetPosition(rmt::Vector*){assert(false);}
    void GetTarget(rmt::Vector*){assert(false);}
} active;
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
    const float initial=StereoDepth::FurthestScreenPlane(1);
    StereoDepth::State state;
    assert(state.convergence==initial);
    assert(state.Shift(-1)==0 && state.Shift(0)==0 && state.Shift(.01f)==0);
    assert(state.Shift(1)==.02f && state.Shift(2)==.02f);
    assert(state.Shift(std::numeric_limits<float>::quiet_NaN())==0);
    float previous=0;
    for(unsigned step=0;step<=10000;++step){
        const float slider=float(step)/10000;
        const float value=state.Shift(slider);
        assert(std::isfinite(value) && value>=previous && value<=.02f);
        previous=value;
    }
    for(float slider:{.25f,.5f,.75f})
        assert(std::fabs(state.Shift(slider)/.02f-(slider-.01f)/.99f)<.000001f);
    state.Update(StereoDepth::Walking,0,1);assert(state.convergence==initial);
    for(unsigned frame=0;frame<400;++frame)state.Update(StereoDepth::Walking,16,1);
    assert(std::fabs(state.convergence-1)<.001f);
    StereoDepth::State slow=state,bounded=state;
    slow.Update(StereoDepth::Driving,5000,8);bounded.Update(StereoDepth::Driving,50,8);
    assert(slow.convergence==bounded.convergence && slow.convergence<initial);
    for(float nearPlane:{.1f,1.0f,2.5f,10.0f}){
        for(auto camera:{StereoDepth::Driving,StereoDepth::Walking,StereoDepth::Cinematic,StereoDepth::Close}){
            for(float subject:{.01f,1.0f,4.0f,8.0f,100.0f}){
                for(unsigned frame=0;frame<400;++frame)state.Update(camera,33,subject,nearPlane);
                const float plane=state.convergence;
                assert(plane>=nearPlane && plane<=StereoDepth::FurthestScreenPlane(nearPlane));
                // All visible depths, including near clipping and the horizon,
                // share one linear projection with this conservative budget.
                for(float depth:{nearPlane,nearPlane*1.01f,nearPlane*2,nearPlane*100000}){
                    const float pairPixels=400*state.Shift(1)*(1-plane/depth);
                    assert(pairPixels>=-6.0001f && pairPixels<=8.0001f);
                }
                if((camera==StereoDepth::Driving || camera==StereoDepth::Walking) && subject>=nearPlane){
                    const float pairPixels=400*state.Shift(1)*(1-plane/subject);
                    assert(pairPixels>=-.001f); // Never force the player out of the window.
                }
            }
        }
    }
    state.Update(StereoDepth::Walking,16,8,.1f);
    assert(state.convergence<=.17501f); // New interior/close camera is safe on its first frame.
    assert(StereoDepth::ValidNearPlane(std::numeric_limits<float>::quiet_NaN())==1);
    assert(StereoDepth::ValidNearPlane(std::numeric_limits<float>::infinity())==1);
    assert(StereoDepth::ValidNearPlane(0)==1);
    assert(std::isfinite(StereoDepth::ScreenDistance(StereoDepth::Walking,std::numeric_limits<float>::quiet_NaN())));
    for(float plane:{.66f,.95f,4.5f}){
        const float scale=StereoDepth::ProjectionScale(initial,plane);
        for(float depth:{.1f,.5f,1.0f,4.5f,7.0f,100.0f}){
            const float x=.2f*depth,z=.4f*depth;
            assert(std::fabs((x*scale)/(depth*scale)-x/depth)<.000001f);
            assert(std::fabs((z*scale)/(depth*scale)-z/depth)<.000001f);
            assert(std::fabs(.02f*(1-initial/(depth*scale))-.02f*(1-plane/depth))<.000001f);
        }
    }
    const float panelPairPixels=400*.02f*(1-.95f/.5f);
    assert(panelPairPixels<-6 && panelPairPixels>-8); // Entire card stays ahead of any world surface.
    const float artPairPixels=400*.02f*(1-.66f/.5f);
    assert(artPairPixels<0 && artPairPixels>-3);
    Graphics3DS::Initialize(Graphics3DS::Old3DS);
    Graphics3DS::UpdateStereoCamera(StereoDepth::Walking,5000,1);
    assert(Graphics3DS::StereoConvergence()==initial);
    char app[]="test",stereo[]="graphics_stereo=1";char* args[]={app,stereo};
    Graphics3DS::Initialize(Graphics3DS::Old2DS,2,args);
    assert(!Graphics3DS::Stereo() && Graphics3DS::StereoConvergence()==initial);
    managerAllowed=false;updateCamera(CONTEXT_GAMEPLAY,16);
    Graphics3DS::Initialize(Graphics3DS::Old3DS,2,args);
    updateCamera(CONTEXT_LOADING,16); // Never access uninitialized gameplay singletons.
    managerAllowed=true;
    for(auto type:{SuperCam::FOLLOW_CAM,SuperCam::WALKER_CAM,SuperCam::ANIMATED_CAM,SuperCam::FIRST_PERSON_CAM}){
        active.type=type;
        viewCamera.nearPlane=.1f;
        for(unsigned frame=0;frame<400;++frame)updateCamera(CONTEXT_GAMEPLAY,16);
        assert(Graphics3DS::StereoConvergence()<=.17501f);
    }
    // Actual forward player depth, not diagonal range or look-ahead, owns focus.
    viewCamera.nearPlane=1;active.type=SuperCam::WALKER_CAM;avatar.position={100,50,1.5f};
    for(unsigned frame=0;frame<400;++frame)updateCamera(CONTEXT_GAMEPLAY,16);
    assert(std::fabs(Graphics3DS::StereoConvergence()-1.125f)<.001f);
    avatarAvailable=false;cameraAvailable=false;
    for(unsigned frame=0;frame<400;++frame)updateCamera(CONTEXT_GAMEPLAY,16);
    assert(std::fabs(Graphics3DS::StereoConvergence()-initial)<.001f);
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
print('PASS: proportional slider, forward player depth, whole-frustum depth budget, camera/interior cuts, panel occlusion, projection invariance and mono/2DS isolation.')
