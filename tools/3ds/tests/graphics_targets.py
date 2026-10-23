#!/usr/bin/env python3
"""Run production target replacement and viewport scaling with an instrumented SDK."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
display = (root / "libs/pure3d/pddi/pica/display.cpp").read_text()
start = display.index("bool picaDisplay::ApplyGraphicsMode(void)")
replacement = display[start:display.index("\n#endif", start)]
context = (root / "libs/pure3d/pddi/pica/context.cpp").read_text()
start = context.index("void picaContext::SetupViewport(void)")
viewport = context[start:context.index("\nvoid picaContext::ApplyViewport", start)]
fixture = r'''
#include <main/graphics3ds.h>
#include <cassert>
#include <cstddef>
using u32=unsigned;
#define GX_TRANSFER_FLIP_VERT(x) 0
#define GX_TRANSFER_OUT_TILED(x) 0
#define GX_TRANSFER_RAW_COPY(x) 0
#define GX_TRANSFER_IN_FORMAT(x) 0
#define GX_TRANSFER_OUT_FORMAT(x) 0
#define GX_TRANSFER_SCALING(x) 0
enum {GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8,GFX_TOP,GFX_LEFT,GFX_RIGHT};
const int PICA_SCREEN_WIDTH=400,PICA_SCREEN_HEIGHT=240,PICA_BOTTOM_HEIGHT=240,PICA_BOTTOM_WIDTH=320;
struct C3D_RenderTarget {int width;};
bool synced=false,failWide=false,failRight=false,wide=false,stereo=false;
bool readyStereo=true,shaderStereo=false;
unsigned allocations=0,deleted=0;
bool queueDone=false;
void C3Di_RenderQueueWaitDone(){queueDone=true;}
void C3D_FrameSync(){assert(queueDone);synced=true;}
C3D_RenderTarget* C3D_RenderTargetCreate(int h,int w,int,int){
 assert(synced&&h==240);++allocations;
 if((w==800&&failWide)||(w==400&&failRight&&allocations%2==0))return nullptr;
 return new C3D_RenderTarget{w};
}
void C3D_RenderTargetDelete(C3D_RenderTarget* p){assert(synced);++deleted;delete p;}
void C3D_RenderTargetSetOutput(C3D_RenderTarget* p,int,int,u32){assert(p);}
void gfxSet3D(bool on){stereo=on;wide=false;}
void gfxSetWide(bool on){assert(!stereo);wide=on;}
void picaRequest3DIndicator(){}
struct Context {
 bool StereoShadersReady(){return readyStereo;}
 void PrepareFramePrograms(){assert(synced && queueDone);shaderStereo=Graphics3DS::Stereo();}
};
struct picaDisplay {
 int renderWidth=400,winHeight=240;bool reset=false;
 C3D_RenderTarget* target=new C3D_RenderTarget{400};C3D_RenderTarget* targetRight=nullptr;
 Context storage;Context* context=&storage;
 bool ApplyGraphicsMode();
 int GetWidth(){return 400;}int GetHeight(){return 240;}
 int RenderWidth(){return renderWidth;}bool world=false;
 bool IsWorldTarget(){return world;}int WorldWidth(){return 320;}int WorldHeight(){return 192;}
 ~picaDisplay(){delete target;delete targetRight;}
};
enum {PDDI_PROJECTION_DEVICE,PDDI_PROJECTION_PERSPECTIVE};
struct View {int projectionMode=PDDI_PROJECTION_DEVICE;struct {float left=0,top=0,right=1,bottom=1;} viewWindow;};
struct picaContext {
 bool bottomPass=false;picaDisplay* display;int wantViewport[4]={};
 struct {View* viewState;} state;
 void ApplyViewport(){}void SetupViewport();
};
'''
main = r'''
int main(){
 using namespace Graphics3DS;
 char app[]="test",normal[]="graphics_quality=0";char* normalArgs[]={app,normal};
 Initialize(Old3DS,2,normalArgs);picaDisplay d;assert(d.ApplyGraphicsMode());
 assert(allocations==0&&!wide&&!stereo&&d.RenderWidth()==400);
 RequestQuality(true);synced=false;assert(d.ApplyGraphicsMode());
 assert(synced&&wide&&!stereo&&d.target->width==800&&d.RenderWidth()==800);
 View v;picaContext c;c.display=&d;c.state.viewState=&v;
 c.SetupViewport();assert(c.wantViewport[2]==240&&c.wantViewport[3]==800);
 assert(d.GetWidth()==400&&d.GetHeight()==240);
 v.projectionMode=PDDI_PROJECTION_PERSPECTIVE;v.viewWindow.left=.25f;v.viewWindow.right=.75f;
 c.SetupViewport();assert(c.wantViewport[1]==200&&c.wantViewport[3]==400);
 c.bottomPass=true;c.SetupViewport();assert(c.wantViewport[2]==240&&c.wantViewport[3]==320);
 c.bottomPass=false;d.world=true;c.SetupViewport();
 assert(c.wantViewport[1]==80&&c.wantViewport[2]==192&&c.wantViewport[3]==160);d.world=false;
 RequestQuality(false);synced=false;assert(d.ApplyGraphicsMode());assert(!wide&&d.RenderWidth()==400);
 failWide=true;RequestQuality(true);synced=false;assert(d.ApplyGraphicsMode());
 assert(!Quality()&&!wide&&d.target->width==400&&d.RenderWidth()==400);
 Initialize(Old2DS);RequestQuality(true);unsigned before=allocations;assert(d.ApplyGraphicsMode());
 assert(Quality()&&ImprovedLighting()&&!wide&&allocations==before);
 Initialize(New2DSXL);failWide=false;assert(d.ApplyGraphicsMode());assert(wide&&!stereo);
 char executable[]="test",arg[]="graphics_stereo=1";char* argv[]={executable,arg};
 Initialize(Old3DS,2,argv);assert(d.ApplyGraphicsMode());assert(stereo&&!wide&&d.targetRight);
 Initialize(Old3DS);assert(d.ApplyGraphicsMode());assert(!stereo&&!wide&&!d.targetRight);
 Initialize(Old3DS);RequestQuality(true);assert(d.ApplyGraphicsMode());
 AllowChanges(false);
 for(unsigned cycle=0;cycle<32;++cycle){
  UpdateSlider(.25f);assert(CanApply());synced=false;assert(d.ApplyGraphicsMode());
  assert(stereo&&shaderStereo&&!wide&&d.targetRight&&d.RenderWidth()==400);
  const unsigned before=allocations;
  UpdateSlider(.75f);assert(!Pending());assert(d.ApplyGraphicsMode());assert(allocations==before);
  UpdateSlider(0);assert(CanApply());synced=false;assert(d.ApplyGraphicsMode());
  assert(!stereo&&!shaderStereo&&wide&&!d.targetRight&&d.RenderWidth()==800);
 }
 readyStereo=false;UpdateSlider(1);assert(d.ApplyGraphicsMode());
 assert(!stereo&&!shaderStereo&&!wide&&!d.targetRight&&d.RenderWidth()==400);
 const unsigned failedAllocations=allocations;
 UpdateSlider(1);assert(!Pending());assert(d.ApplyGraphicsMode());assert(allocations==failedAllocations);
 readyStereo=true;UpdateSlider(0);assert(d.ApplyGraphicsMode());assert(wide);
 failRight=true;UpdateSlider(1);assert(d.ApplyGraphicsMode());
 assert(!stereo&&!shaderStereo&&!d.targetRight&&d.RenderWidth()==400);
}
'''
with tempfile.TemporaryDirectory(prefix="srr-targets-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(fixture + replacement + viewport + main)
    binary = path / "test"
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-DSRR2_OLD_3DS",
        "-fsanitize=address,undefined", "-I", str(root / "code"), str(path / "test.cpp"),
        str(root / "code/main/graphics3ds.cpp"), str(root / "code/cheats/portsettings.cpp"),
        "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], cwd=path, check=True)
print("PASS: target switching, GPU retirement, live slider cycles, shader/right-eye failure recovery and logical/top/bottom/world viewports.")
