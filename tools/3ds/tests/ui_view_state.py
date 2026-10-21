#!/usr/bin/env python3
"""Exercise the production radar pass against a view that replaces camera and matrix state."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/presentation/gui/utility/hudmap.cpp').read_text()
start = source.index('void CHudMap::DisplayOnBottomScreen( tView* guiView, Scrooby::Group* background )')
end = source.index('\n#endif', start)
production = source[start:end]
guard_start = source.index('    class NativeViewPass')
guard_end = source.index('    void DrawUpperPreview', guard_start)
production += '\n' + source[guard_start:guard_end]

fixture = r'''
#include <cassert>
#include <vector>
#include <cstdio>
enum {PDDI_STATE_RENDER,PDDI_STATE_VIEW,PDDI_PROJECTION_PERSPECTIVE,PDDI_CULL_NONE};
struct tView;
struct Matrix {
    int value=42;std::vector<int> saved;
    void Push(){saved.push_back(value);}
    void Pop(){assert(!saved.empty());value=saved.back();saved.pop_back();}
    void LoadIdentity(){value=0;}
    void Translate(float,float,float){value=7;}
};
struct Device {
    float camera=0.1f;bool depth=true;
    std::vector<float> views;std::vector<bool> renders;
    void PushState(int state){if(state==PDDI_STATE_VIEW)views.push_back(camera);else renders.push_back(depth);}
    void PopState(int state){if(state==PDDI_STATE_VIEW){camera=views.back();views.pop_back();}else{depth=renders.back();renders.pop_back();}}
    void EnableZBuffer(bool value){depth=value;}
    void SetCamera(float near,float,float,float){camera=near;}
    void SetProjectionMode(int){}
    void SetCullMode(int){}
};
struct Display {bool wide=false;bool IsWidescreen(){return wide;}};
struct Context {tView* view=nullptr;tView* GetView(){return view;}void SetView(tView* value){view=value;}};
namespace p3d {Matrix* stack;Device* pddi;Display* display;Context* context;}
struct tView {
    unsigned begins=0,ends=0,clears=0,clearMask=3;
    unsigned GetClearMask(){return clearMask;}void SetClearMask(unsigned value){clearMask=value;}
    void BeginRender(){++begins;if(clearMask)++clears;p3d::pddi->camera=2;p3d::stack->value=91;p3d::context->view=this;}
    void EndRender(){++ends;p3d::context->view=nullptr;}
};
namespace Scrooby {
struct Group {unsigned draws=0;};
struct App {static App* GetInstance(){static App app;return &app;}int GetScreenWidth(){return 640;}int GetScreenHeight(){return 480;}};
}
struct Object {bool corrected=true;void SetWideScreenCorrectionEnabled(bool enabled){corrected=enabled;}};
void DisplayChild(Scrooby::Group* group){++group->draws;}
struct CHudMap {
    Object* m_p3dMap;Object* m_p3dHole;Scrooby::Group* m_hudMapGroup;
    void DisplayOnBottomScreen(tView*,Scrooby::Group*);
};
'''
main = r'''
int main(){
 Matrix matrix;Device device;Display display;Context context;
 p3d::stack=&matrix;p3d::pddi=&device;p3d::display=&display;p3d::context=&context;
 Object map,hole;Scrooby::Group icons,background;CHudMap radar{&map,&hole,&icons};
 tView caller,view;
 for(bool wide:{false,true})for(bool withBackground:{false,true})for(bool withCaller:{false,true}){
  display.wide=wide;context.view=withCaller?&caller:nullptr;
  const unsigned before=icons.draws;const unsigned backdrop=background.draws;
  radar.DisplayOnBottomScreen(&view,withBackground?&background:nullptr);
  assert(device.camera==0.1f); // Native UI at z=0.5 must survive the map's near plane of 2.
  assert(device.depth && matrix.value==42 && matrix.saved.empty());
  assert(device.views.empty() && device.renders.empty());
  assert(context.view==(withCaller?&caller:nullptr));
  assert(icons.draws==before+1 && background.draws==backdrop+(withBackground?1:0));
  assert(map.corrected && hole.corrected);
 }
 assert(view.begins==8 && view.ends==8);
 // Exercise the exact guard shared by HUD, menus and upper previews.
 for(bool withCaller:{false,true}){
  context.view=withCaller?&caller:nullptr;
  {NativeViewPass pass(&view);assert(matrix.value==91 && device.camera==2);
   matrix.value=123;device.camera=0.2f;device.depth=false;pass.End();pass.End();}
  assert(matrix.value==42 && device.camera==0.1f && device.depth);
  assert(context.view==(withCaller?&caller:nullptr));
  assert(matrix.saved.empty() && device.views.empty() && device.renders.empty());
 }
 assert(view.begins==10 && view.ends==10 && view.clears==0 && view.clearMask==3);
 puts("PASS: production radar pass restores native camera, matrix, depth, view ownership and widescreen correction in all eight cases.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-ui-view-') as temporary:
    directory = Path(temporary)
    source_file = directory / 'test.cpp'
    source_file.write_text(fixture + production + main)
    binary = directory / 'test'
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    str(source_file), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

# A map draw updates the hardware clip. Returning to the native canvas must
# restore the logical full-screen clip even when that logical state did not change.
context = (root / 'libs/pure3d/pddi/pica/context.cpp').read_text()
start = context.index('void picaContext::SetBottomMapping(')
end = context.index('\nextern "C"', start)
mapping = context[start:end]
fixture = r'''
#include <cassert>
#include <cstring>
struct Rect {int left=0,top=0,right=400,bottom=240;};
struct View {Rect scissor;};
struct picaContext {
 bool bottomPass=true;float bottomSrc[4]={},bottomDst[4]={};
 View view;struct {View* viewState;} state{&view};
 int clips=0;float hardwareRight=0;
 void SetupHardwareProjection(){}
 void InvalidateUniformCache(){}
 void SetScissor(Rect*){++clips;hardwareRight=bottomDst[2];}
 void SetBottomMapping(const float*,float,float,float,float);
};
'''
main = r'''
int main(){
 picaContext context;const float source[4]={0,0,1,1};
 context.SetBottomMapping(source,80,32,240,192);
 assert(context.hardwareRight==0.75f);
 context.SetBottomMapping(source,0,0,320,240);
 assert(context.hardwareRight==1 && context.clips==2);
 context.bottomPass=false;context.SetBottomMapping(source,0,0,1,1);
 assert(context.clips==2);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-ui-clip-') as temporary:
    directory = Path(temporary)
    source_file = directory / 'test.cpp'
    source_file.write_text(fixture + mapping + main)
    binary = directory / 'test'
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    str(source_file), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    print('PASS: production mapping restores hardware clip after radar crop.')
