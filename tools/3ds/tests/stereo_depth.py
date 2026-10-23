#!/usr/bin/env python3
"""Exercise production camera anchors, obstruction recovery and UI isolation."""
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
struct InteriorManager {bool inside=false;bool IsInside(){return inside;}} interior;
InteriorManager* GetInteriorManager(){assert(managerAllowed);return &interior;}
struct SuperCam {
    enum Type {FOLLOW_CAM, NEAR_FOLLOW_CAM, FAR_FOLLOW_CAM, CHASE_CAM, REVERSE_CAM,
        WRECKLESS_CAM, BURNOUT_CAM, SUPER_SPRINT_CAM, WALKER_CAM, BUMPER_CAM,
        FIRST_PERSON_CAM, DEFAULT_CAM, ANIMATED_CAM};
    Type type=FOLLOW_CAM;bool transitioning=false,obstructed=false;
    Type GetType(){return type;}
    bool IsTransitioning(){return transitioning;}
    bool IsObstructed(){return obstructed;}
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
void settle(StereoDepth::State& state,StereoDepth::Camera camera,float subject,float nearPlane=1,unsigned key=0){
    for(unsigned frame=0;frame<400;++frame)state.Update(camera,16,subject,nearPlane,key);
}
int main(){
    const float initial=StereoDepth::FurthestScreenPlane(1);
    StereoDepth::State state;
    assert(state.convergence==initial);
    assert(state.Shift(-1)==0 && state.Shift(.01f)==0 && state.Shift(1)==.02f);
    assert(state.Shift(std::numeric_limits<float>::quiet_NaN())==0);
    float previous=0;
    for(unsigned step=0;step<=10000;++step){
        const float value=state.Shift(float(step)/10000);
        assert(std::isfinite(value) && value>=previous && value<=.02f);previous=value;
    }
    for(float slider:{.25f,.5f,.75f})
        assert(std::fabs(state.Shift(slider)/.02f-(slider-.01f)/.99f)<.000001f);
    state.Update(StereoDepth::Walking,0,6.278061f);assert(state.convergence==initial);
    settle(state,StereoDepth::Walking,6.278061f);
    assert(std::fabs(state.convergence-6.278061f*.95f)<.001f);
    assert(std::fabs(400*state.Shift(1)*(1-state.convergence/6.278061f)-.4f)<.002f);
    const float anchored=state.convergence;
    // Running, jumping and small subject-distance changes cannot chase focus.
    for(unsigned frame=0;frame<2000;++frame)state.Update(StereoDepth::Walking,16,6.278061f+std::sin(float(frame))*.4f);
    assert(std::fabs(state.convergence-anchored)<.001f);
    // Distinct driving cameras retain distinct anchors, including one profile.
    settle(state,StereoDepth::Driving,9.627563f,1,1);
    assert(std::fabs(state.convergence-9.627563f*.95f)<.001f);
    settle(state,StereoDepth::Driving,6,1,2);
    assert(std::fabs(state.convergence-5.7f)<.001f);
    // Deliberate transitions follow the rendered camera, then hold still.
    state.Update(StereoDepth::Driving,16,7,1,2,true);
    state.Update(StereoDepth::Driving,16,8,1,2,true);
    state.Update(StereoDepth::Driving,16,9,1,2,false);
    settle(state,StereoDepth::Driving,9.2f,1,2);
    assert(std::fabs(state.convergence-8.55f)<.001f);
    for(float nearPlane:{.1f,1.f,2.5f}){
        state.Reset();settle(state,StereoDepth::Walking,6.278061f,nearPlane);
        state.Update(StereoDepth::Walking,16,6.278061f,nearPlane,0,false,true);
        assert(state.convergence<=StereoDepth::FurthestScreenPlane(nearPlane));
        for(float z:{nearPlane,nearPlane*1.01f,nearPlane*2,nearPlane*100000}){
            const float pixels=400*state.Shift(1)*(1-state.convergence/z);
            assert(pixels>=-6.0001f && pixels<=8.0001f);
        }
        const float guarded=state.convergence;
        for(unsigned frame=0;frame<10;++frame)state.Update(StereoDepth::Walking,16,6.278061f,nearPlane);
        assert(state.convergence==guarded); // Contact flicker cannot pump depth.
        settle(state,StereoDepth::Walking,6.278061f,nearPlane);
        assert(std::fabs(state.convergence-6.278061f*.95f)<.001f);
    }
    state.Reset();settle(state,StereoDepth::Walking,6.278061f);
    state.Update(StereoDepth::Walking,16,2);
    assert(state.convergence<=initial); // Compression is safe without a contact flag.
    state.Reset();settle(state,StereoDepth::Interior,4,.1f);
    assert(std::fabs(state.convergence-3.6f)<.001f);
    for(auto camera:{StereoDepth::Cinematic,StereoDepth::Close,StereoDepth::Default,StereoDepth::Paused}){
        settle(state,camera,6,1);state.Update(camera,16,6,.1f);
        assert(state.convergence<=.17501f);
    }
    state.Reset();state.Update(StereoDepth::Walking,16,0);
    settle(state,StereoDepth::Walking,6.278061f);
    assert(state.convergence>5.9f); // Uninitialized cameras cannot lock in a fallback.
    StereoDepth::State slow=state,bounded=state;
    slow.Update(StereoDepth::Driving,5000,9);bounded.Update(StereoDepth::Driving,50,9);
    assert(slow.convergence==bounded.convergence);
    assert(StereoDepth::ValidNearPlane(std::numeric_limits<float>::quiet_NaN())==1);
    assert(std::isfinite(StereoDepth::ScreenDistance(StereoDepth::Walking,std::numeric_limits<float>::quiet_NaN())));
    for(float worldPlane:{.175f,1.75f,5.964f,9.146f})for(float objectPlane:{.66f,.95f,4.8f}){
        const float scale=StereoDepth::ProjectionScale(worldPlane,objectPlane);
        for(float z:{.5f,1.f,4.5f,7.f,100.f}){
            assert(std::fabs(.02f*(1-worldPlane/(z*scale))-.02f*(1-objectPlane/z))<.000001f);
            assert(std::fabs((.2f*z*scale)/(z*scale)-.2f)<.000001f);
        }
    }
    assert(std::fabs(400*.02f*(1-.95f/.5f)+7.2f)<.00001f); // Accepted pause card.
    Graphics3DS::Initialize(Graphics3DS::Old3DS);
    Graphics3DS::UpdateStereoCamera(StereoDepth::Walking,16,6);assert(Graphics3DS::StereoConvergence()==initial);
    char app[]="test",stereo[]="graphics_stereo=1";char* args[]={app,stereo};
    Graphics3DS::Initialize(Graphics3DS::Old2DS,2,args);managerAllowed=false;updateCamera(CONTEXT_GAMEPLAY,16);
    Graphics3DS::Initialize(Graphics3DS::Old3DS,2,args);updateCamera(CONTEXT_LOADING,16);
    managerAllowed=true;active.type=SuperCam::WALKER_CAM;avatar.position={100,50,6.278061f};
    for(unsigned frame=0;frame<400;++frame)updateCamera(CONTEXT_GAMEPLAY,16);
    assert(std::fabs(Graphics3DS::StereoConvergence()-6.278061f*.95f)<.001f);
    active.obstructed=true;updateCamera(CONTEXT_GAMEPLAY,16);assert(Graphics3DS::StereoConvergence()<=initial);
    active.obstructed=false;active.type=SuperCam::NEAR_FOLLOW_CAM;avatar.position.z=9.627563f;
    for(unsigned frame=0;frame<400;++frame)updateCamera(CONTEXT_GAMEPLAY,16);
    assert(std::fabs(Graphics3DS::StereoConvergence()-9.627563f*.95f)<.001f);
    updateCamera(CONTEXT_SUPERSPRINT,16);
    assert(Graphics3DS::StereoConvergence()<=initial);
    updateCamera(CONTEXT_DEMO,16);
    assert(Graphics3DS::StereoConvergence()<=initial);
    updateCamera(CONTEXT_PAUSE,16);
    assert(Graphics3DS::StereoConvergence()<=initial); // World stays behind the foreground card.
    active.type=SuperCam::WALKER_CAM;interior.inside=true;avatar.position.z=4;viewCamera.nearPlane=.1f;
    for(unsigned frame=0;frame<400;++frame)updateCamera(CONTEXT_GAMEPLAY,16);
    assert(std::fabs(Graphics3DS::StereoConvergence()-3.6f)<.001f);
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
print('PASS: player anchoring, fixed shot focus, camera variants/transitions, collision safety/recovery, interiors, unchanged UI depth, proportional slider and mono/2DS isolation.')
