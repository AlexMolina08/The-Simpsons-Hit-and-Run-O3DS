#!/usr/bin/env python3
"""Exercise production fog preparation, pass boundaries and LUT cache reuse."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/pure3d/pddi/pica/context.cpp').read_text()

def function(signature, text=source):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'

harness = r'''
#define SRR2_OLD3DS_NATIVE_BENCH
#define SRR2_OLD3DS_DISTANCE_HAZE
#include <pddi/pica/distancehaze.hpp>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
namespace picaDistanceHaze {bool enabled=true;unsigned builds=0,passes=0;float nearPlane=0,farPlane=0;}
namespace Old3dsPerformance {bool enabled=true;bool Enabled(){return enabled;}}
enum {PDDI_PROJECTION_PERSPECTIVE,PDDI_PROJECTION_ORTHOGRAPHIC,GPU_NO_FOG,GPU_FOG,GPU_PLAIN_DENSITY};
struct pddiColour {
 unsigned r,g,b;pddiColour(unsigned r=0,unsigned g=0,unsigned b=0):r(r),g(g),b(b){}
 unsigned Red()const{return r;}unsigned Green()const{return g;}unsigned Blue()const{return b;}
};
struct FogState {bool enabled=false;float start=0,end=0;pddiColour colour;};
struct Camera {float nearPlane=1,farPlane=200;};
struct View {Camera camera;int projectionMode=PDDI_PROJECTION_PERSPECTIVE;};
struct State {FogState fog;View view;FogState* fogState=&fog;View* viewState=&view;};
struct pddiBaseContext {
 State state;
 void EnableFog(bool enabled){state.fog.enabled=enabled;}
 void SetFog(pddiColour colour,float start,float end){state.fog.colour=colour;state.fog.start=start;state.fog.end=end;}
};
struct C3D_FogLut {unsigned data[128];};
static bool g_picaInFrame=true,flipped=false;static unsigned binds=0,modes=0,mode=GPU_NO_FOG,colour=0;
static float captured[256];
float FogLut_CalcZ(float depth,float n,float f){return f*n/(depth*(f-n)+n);}
void FogLut_FromArray(C3D_FogLut* lut,const float* data){
 for(unsigned i=0;i<256;++i)captured[i]=data[i];
 // SDK layout: visibility in bits 13..23 and signed interpolation delta below.
 for(unsigned i=0;i<128;++i){unsigned v=unsigned(data[i]*2048);if(v>2047)v=2047;
  const int delta=int(data[i+128]*2048);lut->data[i]=(v<<13)|(unsigned(delta)&0x1fff);}
}
void C3D_FogLutBind(C3D_FogLut*){++binds;}
void C3D_FogColor(unsigned value){colour=value;}
void C3D_FogGasMode(unsigned value,unsigned,bool flip){mode=value;flipped=flip;++modes;}
struct picaContext:pddiBaseContext {
 bool bottomPass=false,hazeValid=false,hazeActive=false,fogLutValid=false;
 float hazeNear=0,hazeFar=0,fogLutStart=0,fogLutEnd=0;
 unsigned hazeColour=0;C3D_FogLut hazeLut,fogLut;
 void BeginDistanceHaze(unsigned,unsigned);void EndDistanceHaze();void ApplyFogState();
};
float Tan(float value){return std::tan(value);}
struct Row {float x,y,z,w;void Set(float a,float b,float c,float d){x=a;y=b;z=c;w=d;}};
struct Matrix {Row rows[4];Row& Row4(unsigned i){return rows[i];}void SetPerspective(float,float,float,float);};
'''
tests = r'''
int main(){
 Matrix projection;projection.SetPerspective(1.2f,1.6667f,1.0f,8000.0f);
 const float samples[]={1.f,4.f,16.f,45.f,70.f,120.f,200.f,8000.f};
 for(float z:samples){
  // Production ApplyProjection negates the Pure3D z column. The viewport
  // maps that negative clip depth to 0 at near and 1 at far.
  const float clipZ=-(projection.rows[2].z*z+projection.rows[3].z);
  const float deviceDepth=-clipZ/z;
  const float fogDepth=1-deviceDepth;
  assert(std::fabs(FogLut_CalcZ(fogDepth,1,8000)-z)<z*.001f);
  const float visibility=picaDistanceHaze::Visibility(fogDepth,1,8000);
  if(z<=45)assert(visibility>.9999f);
  if(z>=120)assert(visibility<.87f);
 }
 picaContext c;
 c.BeginDistanceHaze(2,0xdec29a);assert(binds==0&&!c.hazeActive);
 picaDistanceHaze::enabled=false;c.BeginDistanceHaze(0,0xdec29a);assert(binds==0);
 picaDistanceHaze::enabled=true;
 for(unsigned frame=0;frame<10000;++frame){
  c.BeginDistanceHaze(0,0xdec29a);
  assert(c.hazeActive&&c.state.fog.enabled&&colour==0xdec29a&&mode==GPU_FOG);
  assert(flipped);
  assert(binds==1&&picaDistanceHaze::builds==1);
  c.ApplyFogState();assert(mode==GPU_FOG&&flipped);
  c.bottomPass=true;c.ApplyFogState();assert(mode==GPU_NO_FOG);
  c.bottomPass=false;c.ApplyFogState();assert(mode==GPU_FOG);
  c.state.view.projectionMode=PDDI_PROJECTION_ORTHOGRAPHIC;
  c.ApplyFogState();assert(mode==GPU_NO_FOG);
  c.state.view.projectionMode=PDDI_PROJECTION_PERSPECTIVE;
  c.ApplyFogState();assert(mode==GPU_FOG);
  c.pddiBaseContext::EnableFog(false);c.ApplyFogState();assert(mode==GPU_NO_FOG);
  c.pddiBaseContext::EnableFog(true);c.ApplyFogState();assert(mode==GPU_FOG);
  c.EndDistanceHaze();assert(mode==GPU_NO_FOG&&!c.hazeActive&&!c.state.fog.enabled);
 }
 assert(picaDistanceHaze::passes==10000);
 for(unsigned i=0;i<128;++i){
  assert(captured[i]>=1-picaDistanceHaze::Strength-.000001f&&captured[i]<=1);
  assert(captured[i+128]>=0);
  const float next=i==127?1:captured[i+1];
  assert(std::fabs(captured[i]+captured[i+128]-next)<.000001f);
 }
 unsigned before=modes;c.BeginDistanceHaze(6,0);c.EndDistanceHaze();assert(modes==before);
 c.state.view.camera.nearPlane=.5f;c.BeginDistanceHaze(1,0xdec29a);c.EndDistanceHaze();
 assert(binds==2&&picaDistanceHaze::builds==2);
 c.state.view.camera.nearPlane=0;before=modes;c.BeginDistanceHaze(1,0);assert(modes==before);
 float data[258];data[0]=17;data[257]=23;
 const float bad[][2]={{0,100},{-1,100},{1,1},{2,1},
  {1,std::numeric_limits<float>::infinity()},
  {std::numeric_limits<float>::quiet_NaN(),100},{1.e30f,1.e35f}};
 for(const auto& range:bad)assert(!picaDistanceHaze::Build(data+1,range[0],range[1]));
 assert(picaDistanceHaze::Build(data+1,.1f,1000));assert(data[0]==17&&data[257]==23);
 puts("PASS: 10000 production fog passes; one cached LUT, projection invalidation, correct visibility/delta layout, sky/UI/night exclusions and bounded strength");
}
'''
methods = ''.join(function(signature) for signature in (
    'void picaContext::BeginDistanceHaze(', 'void picaContext::EndDistanceHaze(',
    'void picaContext::ApplyFogState('))
methods = function('void Matrix::SetPerspective(',
                   (root / 'libs/radmath/radmath/matrix.cpp').read_text()) + methods
with tempfile.TemporaryDirectory(prefix='srr-distance-haze-') as td:
    folder = Path(td)
    (folder / 'test.cpp').write_text(harness + methods + tests)
    subprocess.run(['clang++', '-std=c++14', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I' + str(root / 'libs/pure3d'),
                    str(folder / 'test.cpp'), '-o', str(folder / 'test')], check=True)
    subprocess.run([str(folder / 'test')], check=True)
