#!/usr/bin/env python3
"""Execute production light setup/capture/rebuild across changing light states."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3];s=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
a=s.index('void picaContext::SetupHardwareLight(');b=s.index('\nvoid picaContext::SetVertexUniforms(',a);methods=s[a:b]
prelude=r'''
#include <pddi/pica/lightcapture.hpp>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <limits>
#include <cstdint>
const int PDDI_MAX_LIGHTS=4,PDDI_LIGHT_DIRECTIONAL=0;
struct pddiVector{float x=0,y=0,z=0;};
struct Vector4{float x=0,y=0,z=0,w=0;};
struct pddiMatrix{Vector4 rows[4];Vector4& Row4(int i){return rows[i];}};
struct C3D_FVec{float w=0,z=0,y=0,x=0;};
struct pddiColour{unsigned c=0;unsigned Red()const{return c&255;}unsigned Green()const{return(c>>8)&255;}unsigned Blue()const{return(c>>16)&255;}};
struct pddiLight{bool enabled=false;int type=0;pddiColour colour;pddiVector worldDirection,worldPosition;float attenA=0,attenB=0,attenC=0;};
struct Lighting{pddiLight light[4];};
struct Stack{pddiMatrix matrix;pddiMatrix* Top(){return &matrix;}};
struct State{Lighting* lightingState;Stack* matrixStack[1];};
const int PDDI_MATRIX_MODELVIEW=0;
'''
controller=r'''
struct picaContext{
 Lighting lighting;Stack stack;State state={&lighting,{&stack}};
 C3D_FVec lightPos[4],lightColour[4],lightAtten[4],eyeLight[4];
 int nActiveLights=0;bool lightsDirty=true;
 picaLightCapture::Cache lightCaptureCache[4];
 unsigned lightSetupCalls=0,lightDisabledSkips=0,lightCaptureHits=0,lightCaptureComputations=0;
 void SetupHardwareLight(int);void CaptureEyeLight(int);void RebuildLightArray();
};
'''
tests=r'''
static uint32_t state=928314;
static unsigned Next(){state^=state<<13;state^=state>>17;return state^=state<<5;}
int main(){
 Reference::picaContext a;Optimized::picaContext b;
 pddiMatrix matrix;for(unsigned i=0;i<4;++i)matrix.rows[i].w=i==3?1:0;
 pddiVector vector={1,2,3};picaLightCapture::Cache cache;
 assert(!cache.Matches(&matrix,&vector,0,0));
 for(unsigned control=0;control<4;++control){
  cache.Store(&matrix,&vector,0,control,true);assert(cache.Matches(&matrix,&vector,0,control));
  assert(!cache.Matches(&matrix,&vector,0,control^1));assert(!cache.Matches(&matrix,&vector,1,control));
  cache.Store(&matrix,&vector,0,control,false);assert(!cache.Matches(&matrix,&vector,0,control));
 }
 unsigned draws=0,setups=0;
 auto draw=[&](){
  // Original disabled eye slots can differ internally; only enabled slots and
  // the actual packed arrays are consumed by the renderer.
  for(unsigned i=0;i<4;++i)if(a.lighting.light[i].enabled)assert(!memcmp(&a.eyeLight[i],&b.eyeLight[i],sizeof(C3D_FVec)));
  a.RebuildLightArray();b.RebuildLightArray();
  assert(a.nActiveLights==b.nActiveLights);
  assert(!memcmp(a.lightPos,b.lightPos,sizeof(a.lightPos)));
  assert(!memcmp(a.lightColour,b.lightColour,sizeof(a.lightColour)));
  assert(!memcmp(a.lightAtten,b.lightAtten,sizeof(a.lightAtten)));++draws;
 };
 for(unsigned test=0;test<6000;++test){
  for(unsigned row=0;row<4;++row){
   matrix.rows[row].x=(row==0?1.f:0)+float(int(Next()%7)-3)*.02f;
   matrix.rows[row].y=(row==1?1.f:0)+float(int(Next()%7)-3)*.02f;
   matrix.rows[row].z=(row==2?1.f:0)+float(int(Next()%7)-3)*.02f;
  }
  a.stack.matrix=b.stack.matrix=matrix;
  // Lighting-state sync performs eight setter hooks for each slot. A draw
  // between arbitrary hooks also exercises immediate enabling/capture rules.
  for(unsigned i=0;i<4;++i){
   pddiLight light=a.lighting.light[i];
   for(unsigned setter=0;setter<8;++setter){
    if(setter==0)light.enabled=(test>>i)&1;
    if(setter==1)light.type=(test+i)%3==0?1:0;
    if(setter==2)light.colour.c=Next();
    if(setter==3)light.worldDirection={float(int(Next()%100)-50),float(int(Next()%100)-50),float(int(Next()%100)-50)};
    if(setter==4)light.worldPosition={float(int(Next()%100)-50),float(int(Next()%100)-50),float(int(Next()%100)-50)};
    if(setter==6){light.attenA=1;light.attenB=.1f;light.attenC=.01f;}
    if(test%31==0&&setter==3)light.worldDirection={0,0,0};
    if(test%97==0&&setter==4)light.worldPosition.x=std::numeric_limits<float>::infinity();
    a.lighting.light[i]=b.lighting.light[i]=light;
    a.SetupHardwareLight(i);b.SetupHardwareLight(i);++setups;
    if((test+setter)%13==0)draw();
   }
  }
  // Matrix changes after setters must not change the captured light frame.
  a.stack.matrix.rows[3].x=b.stack.matrix.rows[3].x=float(test);draw();
 }
 a.SetupHardwareLight(-1);b.SetupHardwareLight(-1);a.SetupHardwareLight(4);b.SetupHardwareLight(4);
 assert(b.lightSetupCalls==setups&&b.lightCaptureHits>0&&b.lightDisabledSkips>0&&b.lightCaptureComputations<setups/2);
 printf("PASS: %u production light setter hooks, %u packed-array comparisons; exact enabled eye coordinates and light order; disabled skips=%u exact hits=%u computed=%u; cache=%zu bytes/slot\n",setups,draws,b.lightDisabledSkips,b.lightCaptureHits,b.lightCaptureComputations,sizeof(picaLightCapture::Cache));
}
'''
code=prelude+'namespace Reference{\n'+controller+methods+'\n}\n#define SRR2_OLD3DS_LIGHT_CAPTURE\nnamespace Optimized{\n'+controller+methods+'\n}\n'+tests
with tempfile.TemporaryDirectory(prefix='srr-light-capture-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
