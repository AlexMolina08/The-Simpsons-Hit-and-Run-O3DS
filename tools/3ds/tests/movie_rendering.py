#!/usr/bin/env python3
"""Run production movie drawing and transition code against hostile prior state."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]

def method(path, signature):
    source = (root/path).read_text()
    start = source.index(signature)
    cursor = source.index('{', start)+1
    depth = 1
    while depth:
        depth += (source[cursor] == '{')-(source[cursor] == '}')
        cursor += 1
    return source[start:cursor]

production = '\n'.join([
    method('libs/radmovie/src/common/binkrenderstrategy.cpp', 'bool radMovieRenderStrategyBink::Render( void )'),
    method('libs/pure3d/pddi/pica/context.cpp', 'extern "C" void picaBottomScreenBlack(void)'),
    method('code/presentation/fmvplayer/fmvplayer.cpp', 'void FMVPlayer::Play()'),
    method('code/presentation/fmvplayer/fmvplayer.cpp', 'void FMVPlayer::Stop()'),
])
program = r'''
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>
#define rAssert assert
enum {PDDI_STATE_ALL,PDDI_MATRIX_MODELVIEW,PDDI_PROJECTION_DEVICE,PDDI_PROJECTION_ORTHOGRAPHIC,
 PDDI_CULL_NONE,PDDI_BUFFER_COLOUR,PDDI_SP_ISLIT,PDDI_SP_ALPHATEST,PDDI_SP_BASETEX,
 PDDI_SP_BLENDMODE,PDDI_BLEND_NONE,PDDI_SP_FILTER,PDDI_FILTER_BILINEAR,
 PDDI_PRIM_TRIANGLES,PDDI_V_CT,ANIM_LOADED,ANIM_PLAYING,ANIM_STOPPED,CONTEXT_BOOTUP};
struct tColour {int r,g,b,a;tColour(int r=255,int g=255,int b=255,int a=255):r(r),g(g),b(b),a(a){}};
using pddiColour=tColour;
struct pddiRect {int left,top,right,bottom;};
struct State {bool depth=true,write=true,fog=true,stencil=true,colour=false;
 int projection=-1;float nearPlane=200,farPlane=300,left=.2f,top=.2f,right=.8f,bottom=.8f;
 pddiRect scissor={80,40,300,180};tColour clear={255,255,255};};
struct Display {int GetWidth(){return 400;}int GetHeight(){return 240;}void SetForceVSync(bool){}} display;
struct Shader {int lit=1,alpha=1;void SetInt(int p,int v){if(p==PDDI_SP_ISLIT)lit=v;if(p==PDDI_SP_ALPHATEST)alpha=v;}
 void SetTexture(int,void*){}Shader* GetShader(){return this;}} shader;
struct Texture {int GetWidth(){return 512;}int GetHeight(){return 256;}} texture;
struct Device;
struct pddiPrimStream {Device* device;unsigned vertices=0;void Colour(tColour){}void UV(float,float){}void Coord(float,float,float);};
struct Device {
 State state;std::vector<State> saved;int matrices=0,clears=0,draws=0,ends=0;
 bool dirty=false;float minimumX=10000,maximumX=-10000,minimumY=10000,maximumY=-10000;
 pddiPrimStream stream{this};
 void PushState(int){saved.push_back(state);}void PopState(int){assert(!saved.empty());state=saved.back();saved.pop_back();}
 void PushIdentityMatrix(int){++matrices;}void PopMatrix(int){--matrices;}
 void SetViewWindow(float l,float t,float r,float b){state.left=l;state.top=t;state.right=r;state.bottom=b;}
 void SetScissor(pddiRect* p){state.scissor=*p;}
 void SetCamera(float n,float f,float,float){state.nearPlane=n;state.farPlane=f;}
 void EnableZBuffer(bool b){state.depth=b;}void SetZWrite(bool b){state.write=b;}
 void EnableFog(bool b){state.fog=b;}void EnableStencilBuffer(bool b){state.stencil=b;}
 void SetColourWrite(bool r,bool g,bool b,bool a){state.colour=r&&g&&b&&a;}
 void SetClearColour(tColour c){state.clear=c;}
 void Clear(int mask){assert(mask==PDDI_BUFFER_COLOUR && state.left==0 && state.top==0 && state.right==1 && state.bottom==1);
  assert(state.scissor.left==0 && state.scissor.top==0 && state.scissor.right==400 && state.scissor.bottom==240);
  assert(!state.fog && !state.stencil && state.colour && state.clear.r==0 && state.clear.g==0 && state.clear.b==0);++clears;dirty=true;}
 void SetProjectionMode(int p){state.projection=p;}void SetCullMode(int){}
 pddiPrimStream* BeginPrims(Shader*,int,int,int){
#ifdef RAD_3DS
  assert(clears==1 && !state.depth && !state.write && !state.fog && !state.stencil && state.colour);
  assert(state.nearPlane==.1f && state.farPlane==10 && state.projection==PDDI_PROJECTION_DEVICE);
#endif
  stream.vertices=0;return &stream;}
 void EndPrims(pddiPrimStream* p){assert(p->vertices==6);++draws;dirty=true;}
 void EndFrame(){dirty=false;++ends;}
};
void pddiPrimStream::Coord(float x,float y,float){++vertices;device->minimumX=std::min(device->minimumX,x);device->maximumX=std::max(device->maximumX,x);device->minimumY=std::min(device->minimumY,y);device->maximumY=std::max(device->maximumY,y);}
Device device;
struct Context {int swaps=0,stale=0;void SwapBuffers(){++swaps;if(device.dirty)++stale;}} context;
namespace p3d {Device* pddi=&device;Display* display=&::display;Context* context=&::context;}
struct radMovieRenderStrategyBink {Shader* m_pShader=&shader;float m_DisplayMultiplier=1;unsigned m_NumTiles=1;
 struct Tile {Texture* m_pTexture=&texture;int m_PosX=0,m_PosY=0;unsigned m_Width=320,m_Height=240;}m_pTile[1];
 int m_MoviePosX=40,m_MoviePosY=0;bool Render();};
struct picaContext:Device {bool available=true,bottom=false;int begins=0,finishes=0;
 bool BeginBottomScreen(int,int,int,int,bool){++begins;if(!available)return false;bottom=true;return true;}
 void EndBottomScreen(){assert(bottom);bottom=false;++finishes;}Display* GetDisplay(){return &display;}};
picaContext* s_picaContext=nullptr;
struct InputHandler {void SetEnabled(bool){}} handler;
struct Input {unsigned GetMaxControllers(){return 1;}void RegisterMappable(unsigned,InputHandler*){}void UnregisterMappable(unsigned,InputHandler*){}} input;
Input* GetInputManager(){return &input;}
struct Flow {int GetCurrentContext(){return CONTEXT_BOOTUP;}} flow;Flow* GetGameFlow(){return &flow;}
struct Sound {float GetDialogueVolume(){return 1;}void ResumeAfterMovie(){}} sound;Sound* GetSoundManager(){return &sound;}
struct Movie {void SetVolume(float){}void Play(){}} movie;
struct AnimationPlayer {int state=ANIM_LOADED;void Play(){state=ANIM_PLAYING;}void Stop(){state=ANIM_STOPPED;}};
struct FMVPlayer:AnimationPlayer {Movie* m_refIRadMoviePlayer=&movie;InputHandler* m_UserInputHandler=&handler;
 float mElapsedTime=0,mFadeOut=0,mMovieVolume=0;static constexpr float VOLUME_MULTIPLIER=1;
 int GetState(){return state;}bool IsPlaying(){return state==ANIM_PLAYING;}bool GetSkippable(){return true;}
 void FadeScreen(float){device.dirty=true;}void ClearData(){m_refIRadMoviePlayer=nullptr;}void Play();void Stop();};
''' + production + r'''
int main(){
 {
  device=Device{};device.stream.device=&device;shader=Shader{};radMovieRenderStrategyBink renderer;
  assert(renderer.Render() && device.draws==1 && device.matrices==0 && device.saved.empty());
  assert(device.minimumX==40 && device.maximumX==360 && device.minimumY==0 && device.maximumY==240);
  assert(device.state.depth && device.state.write && device.state.fog && device.state.stencil && !device.state.colour);
  assert(device.state.scissor.left==80 && device.state.nearPlane==200 && device.state.projection==-1);
#ifdef RAD_3DS
  assert(device.clears==1 && shader.lit==0 && shader.alpha==0);
#else
  assert(device.clears==0 && shader.lit==1 && shader.alpha==1);
#endif
  context=Context{};FMVPlayer player;player.Play();assert(player.IsPlaying());player.Stop();
  assert(!player.IsPlaying() && player.m_refIRadMoviePlayer==nullptr && context.swaps==2);
#ifdef RAD_3DS
  assert(device.ends==2 && context.stale==0);
#else
  assert(device.ends==0 && context.stale==2);
#endif
  picaContext bottom;s_picaContext=&bottom;picaBottomScreenBlack();
  assert(bottom.begins==1 && bottom.finishes==1 && bottom.clears==1 && !bottom.bottom && bottom.saved.empty());
  assert(bottom.state.fog && bottom.state.stencil && !bottom.state.colour && bottom.state.scissor.left==80);
  bottom.available=false;picaBottomScreenBlack();assert(bottom.begins==2 && bottom.finishes==1);
  s_picaContext=nullptr;picaBottomScreenBlack();
 }
 puts("PASS: production movie state isolation, black margins, bottom draw, transition vertex publication and legacy behavior.");
}
'''
with tempfile.TemporaryDirectory(prefix='movie-rendering-') as directory:
    path = Path(directory)
    (path/'check.cpp').write_text(program)
    for handheld in (False, True):
        command = ['c++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined',
                   '-fno-sanitize-recover=all', '-DRAD_WIN32', str(path/'check.cpp'), '-o', str(path/'check')]
        if handheld:
            command.append('-DRAD_3DS')
        subprocess.run(command, check=True)
        subprocess.run([str(path/'check')], check=True)
