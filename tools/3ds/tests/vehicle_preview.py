#!/usr/bin/env python3
"""Exercise production preview loading, rapid selections, teardown and rotation."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/presentation/gui/ingame/guiscreenrewards.cpp').read_text()
header = (root / 'code/presentation/gui/ingame/guiscreenrewards.h').read_text()
phone = (root / 'code/presentation/gui/ingame/guiscreenphonebooth.h').read_text()

def function(name):
    start = source.index('IGuiScreenRewards::' + name + '(')
    begin = source.index('{', start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return 'void ' + source[start:end] + '\n'

guard = phone[phone.index('#if !defined(RAD_E3)'):phone.index('#endif', phone.index('#if !defined(RAD_E3)')) + 6]
for flags, expected in [(['-DRAD_3DS'], False), ([], True), (['-DRAD_E3'], False)]:
    macros = subprocess.check_output(['clang++', '-E', '-dM', '-x', 'c++', *flags, '-'], input=guard, text=True)
    assert ('#define SRR2_OVERRIDE_CAR_SELECTION' in macros) == expected

getter = re.search(r'tDrawable\* GetVehiclePreview3DS\(\) const \{[^}]+\}', header).group()
model_path = source[source.index('        sprintf( previewObjects[ newSlot ].filenameModel'):source.index('\n    else', source.index('        sprintf( previewObjects[ newSlot ].filenameModel'))]
model_path = model_path[:model_path.rfind('    }')]
fixture = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <presentation/gui/utility/vehiclepreview.h>
#include <main/stereodepth.h>
#define RAD_3DS
#define RAD_WIN32
#define LOAD_DAMAGED_VEHICLE_IMAGES
#define rAssert assert
enum {GUI_SCREEN_ID_PHONE_BOOTH,GUI_WINDOW_STATE_RUNNING,GUI_WINDOW_STATE_INTRO,GUI_WINDOW_STATE_OUTRO};
enum {REWARDS_SCREEN_CLOSING_LIGHT,REWARDS_SCREEN_LIGHT_CLOSED,REWARDS_SCREEN_OPENING_LIGHT};
enum {FILEHANDLER_PURE3D,GMA_LEVEL_OTHER,GMA_LEVEL_MISSION};
const char* REWARDS_INVENTORY_SECTION="rewards";
const char* PHONE_BOOTH_3DMODEL_CARS_DIR="art\\frontend\\dynaload\\cars\\";
const char* PHONE_BOOTH_2DMODEL_CARS_DIR="art\\frontend\\dynaload\\images\\cars2D\\";
const float LIGHT_OPEN_CLOSE_TRANSITION_TIME=500;
struct PreviewObject {char filenameModel[64];int model=0;};
struct tDrawable {int refs=0;} models[2];
struct tRefCounted {static void Assign(tDrawable*& target,tDrawable* value){if(value)++value->refs;if(target)--target->refs;target=value;}};
struct Element {void SetDrawable(void*){}void SetRawSprite(void*){}void SetVisible(bool){}};
std::vector<int> events;
namespace p3d {
struct Pddi {void DrawSync(){events.push_back(1);}} device;
Pddi* pddi=&device;
struct Inventory {
 bool current=false;
 void PushSection(){}void SelectSection(const char*){}void PopSection(){}
 bool GetCurrentSectionOnly(){return current;}
 void SetCurrentSectionOnly(bool value){current=value;}
 void RemoveSectionElements(const char*){assert(!events.empty() && events.back()==1);events.push_back(2);}
} store;
Inventory* inventory=&store;
}
struct Loader {
 std::vector<std::string> requests;
 void AddRequest(int,const char* file,int,const char*,const char*){requests.push_back(file);}
 void* callback=nullptr;
 void AddCallback(void*,void* value){callback=value;}
} loader;
Loader* GetLoadingManager(){return &loader;}
struct IGuiScreenRewards {
 bool m_isLoading=false,m_isLoadingReward=false;
 int m_ID=GUI_SCREEN_ID_PHONE_BOOTH,m_state=GUI_WINDOW_STATE_RUNNING;
 int m_currentState=REWARDS_SCREEN_LIGHT_CLOSED;
 float m_elapsedTime=0;
 Element *m_previewWindow=nullptr,*m_previewImage=nullptr;
 tDrawable* m_vehiclePreview3DS=nullptr;
 const PreviewObject* m_loadingPreview3DS=nullptr;unsigned m_previewRequest3DS=0;
 const PreviewObject* selected=nullptr;
 void Load3DModel(const PreviewObject*);
 void Unload3DModel();
 void SetVehiclePreview3DS(tDrawable*);
 void OnProcessRequestsComplete(void*);
 const PreviewObject* GetCurrentPreviewObject(){return selected;}
 void On3DModelSelectionChange(const PreviewObject* value){Unload3DModel();Load3DModel(value);}
 void On3DModelLoaded(const PreviewObject* value){SetVehiclePreview3DS(&models[value->model]);}
 GETTER
};
'''.replace('GETTER', getter)
program = fixture + '\n'.join(function(name) for name in ('Load3DModel', 'Unload3DModel', 'SetVehiclePreview3DS', 'OnProcessRequestsComplete')) + r'''
int main(){
 IGuiScreenRewards screen;
 PreviewObject a{},b{};std::strcpy(a.filenameModel,"art\\frontend\\dynaload\\cars\\famil_v");
 std::strcpy(b.filenameModel,"art\\frontend\\dynaload\\cars\\marge_v");b.model=1;
 screen.selected=&a;screen.Load3DModel(&a);void* callbackA=loader.callback;
 assert(loader.requests.size()==1); // A garage model has no damaged-image companion.
 assert(loader.requests.back()==std::string(a.filenameModel)+".p3d");
 assert(screen.m_isLoading && !screen.GetVehiclePreview3DS());
 screen.selected=&b;screen.OnProcessRequestsComplete(callbackA);
 assert(screen.m_isLoading && !screen.GetVehiclePreview3DS());
 assert(loader.requests.back()==std::string(b.filenameModel)+".p3d");
 screen.OnProcessRequestsComplete(loader.callback);
 assert(!screen.m_isLoading && screen.GetVehiclePreview3DS()==&models[1]);
 assert(models[0].refs==0 && models[1].refs==1 && !p3d::inventory->current);
 // Rapid navigation must not display an old model with the new name.
 screen.m_isLoading=true;assert(!screen.GetVehiclePreview3DS());
 screen.selected=&a;screen.OnProcessRequestsComplete(loader.callback);
 assert(models[1].refs==0 && !screen.GetVehiclePreview3DS());
 screen.OnProcessRequestsComplete(loader.callback);assert(screen.GetVehiclePreview3DS()==&models[0]);
 screen.Unload3DModel();assert(!screen.GetVehiclePreview3DS() && models[0].refs==0);
 // A completion received after closing the garage must release its contents.
 screen.selected=&b;screen.Load3DModel(&b);screen.m_state=GUI_WINDOW_STATE_OUTRO;
 screen.OnProcessRequestsComplete(loader.callback);assert(!screen.GetVehiclePreview3DS() && models[1].refs==0);
 assert(!screen.m_isLoading);
 // Close/reopen while an earlier load is pending. The old completion cannot
 // clear the new load flag, trigger duplicate requests, or replace its model.
 screen.m_state=GUI_WINDOW_STATE_RUNNING;screen.selected=&a;screen.Load3DModel(&a);
 void* stale=loader.callback;screen.Unload3DModel();screen.m_isLoading=false;
 screen.selected=&b;screen.Load3DModel(&b);void* fresh=loader.callback;
 const auto requests=loader.requests.size();screen.OnProcessRequestsComplete(stale);
 assert(screen.m_isLoading && loader.requests.size()==requests && !screen.GetVehiclePreview3DS());
 screen.OnProcessRequestsComplete(fresh);assert(screen.GetVehiclePreview3DS()==&models[1]);
 screen.OnProcessRequestsComplete(stale);assert(screen.GetVehiclePreview3DS()==&models[1] && loader.requests.size()==requests);
 screen.Unload3DModel();
 // Reusing the same array slot on a new visit still requires the new token.
 screen.selected=&a;screen.Load3DModel(&a);stale=loader.callback;screen.Unload3DModel();screen.m_isLoading=false;
 screen.Load3DModel(&a);fresh=loader.callback;screen.OnProcessRequestsComplete(stale);
 assert(screen.m_isLoading && !screen.GetVehiclePreview3DS());
 screen.OnProcessRequestsComplete(fresh);assert(screen.GetVehiclePreview3DS()==&models[0]);screen.Unload3DModel();
 for(int visit=0;visit<1000;++visit){
  screen.m_state=GUI_WINDOW_STATE_RUNNING;screen.selected=&b;screen.Load3DModel(&b);screen.OnProcessRequestsComplete(loader.callback);
  assert(models[1].refs==1);screen.Unload3DModel();assert(models[1].refs==0);
 }
 float yaw=VehiclePreview::InitialYaw;
 assert(VehiclePreview::Rotate(yaw,33,false,false)==yaw);
 assert(VehiclePreview::Rotate(yaw,33,true,true)==yaw);
 assert(VehiclePreview::Rotate(yaw,5000,false,true)==VehiclePreview::Rotate(yaw,100,false,true));
 assert(VehiclePreview::Rotate(yaw,33,true,false)<yaw);
 assert(VehiclePreview::Rotate(yaw,33,false,true)>yaw);
 for(int step=0;step<100000;++step){yaw=VehiclePreview::Rotate(yaw,33,false,true);assert(std::isfinite(yaw) && std::fabs(yaw)<6.284f);}
 for(float depth:{VehiclePreview::Distance-VehiclePreview::Radius,VehiclePreview::Distance+VehiclePreview::Radius}){
  const float pairPixels=400*StereoDepth::MaximumShift*(1-VehiclePreview::ScreenDistance/depth);
  assert(pairPixels>-6 && pairPixels<2.001f);
 }

 // Every rotated box corner stays clear of flat title/footer and both edges.
 for(float halfX:{.1f,.5f,.9f}) for(float halfZ:{.2f,.7f,1.f}) for(float halfY:{.1f,.4f,.95f}){
  const float horizontal=std::sqrt(halfX*halfX+halfZ*halfZ);
  const float radius=VehiclePreview::FitRadius(horizontal,-halfY,halfY);
  assert(radius>0 && radius<=VehiclePreview::Radius);
  for(int angle=0;angle<360;++angle) for(int corner=0;corner<8;++corner){
   const float yaw=angle*6.283185307f/360;
   float x=(corner&1?halfX:-halfX)*radius;
   float y=(corner&2?halfY:-halfY)*radius;
   float z=(corner&4?halfZ:-halfZ)*radius;
   const float eyeX=std::cos(yaw)*x+std::sin(yaw)*z;
   z=-std::sin(yaw)*x+std::cos(yaw)*z;
   const float eyeY=std::cos(VehiclePreview::Pitch)*y-std::sin(VehiclePreview::Pitch)*z;
   const float depth=VehiclePreview::Distance+std::sin(VehiclePreview::Pitch)*y+std::cos(VehiclePreview::Pitch)*z;
   const float screenY=120-200/std::tan(.5f)*eyeY/depth;
   assert(screenY>=59.999f && screenY<=201.001f);
   for(int eye:{-1,1}){
    const float screenX=200+200/std::tan(.5f)*eyeX/depth+eye*200*StereoDepth::MaximumShift*(1-VehiclePreview::ScreenDistance/depth);
    assert(screenX>=24.999f && screenX<=375.001f);
   }
  }
 }
 // Compile the actual platform-selected asset path assignment.
 PreviewObject previewObjects[1];int newSlot=0;
 struct Reward {const char* GetName(){return "famil_v";}} reward;
 auto* pReward=&reward;
 MODEL_PATH
 assert(std::string(previewObjects[0].filenameModel)=="art\\frontend\\dynaload\\cars\\famil_v");
}
'''.replace('MODEL_PATH', model_path)
with tempfile.TemporaryDirectory(prefix='srr-vehicle-preview-') as directory:
    path = Path(directory)
    (path / 'test.cpp').write_text(program)
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-deprecated-declarations', '-fsanitize=address,undefined',
                    '-I', str(root / 'code'), str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: retail model paths, no 3DS debug selector, delayed and rapid selection callbacks, closed/reopened-screen cleanup, stale callback isolation, 1000 preview lifetimes, bounded rotation, stereo depth and full-turn framing.')
