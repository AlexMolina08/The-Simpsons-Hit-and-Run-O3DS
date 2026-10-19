#!/usr/bin/env python3
"""Run the production light-array builder with exact general-light fallback."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
a=s.index('void picaContext::RebuildLightArray(');b=s.index('\nvoid picaContext::SetVertexUniforms',a);method=s[a:b]
prelude=r'''
#include <pddi/pica/directionallight.hpp>
#include <cassert>
#include <cstring>
#include <cstdio>
const int PDDI_MAX_LIGHTS=4;
struct C3D_FVec{float w=0,z=0,y=0,x=0;};
struct pddiColour{unsigned c=0;unsigned Red()const{return c&255;}unsigned Green()const{return(c>>8)&255;}unsigned Blue()const{return(c>>16)&255;}};
struct pddiLight{bool enabled=false;pddiColour colour;float attenA=0,attenB=0,attenC=0;};
struct Lighting{pddiLight light[4];};
struct State{Lighting* lightingState;};
'''
controller=r'''
struct picaContext{
 Lighting lighting;State state={&lighting};
 C3D_FVec lightPos[4],lightColour[4],lightAtten[4],eyeLight[4],directionalPos[4];
 int nActiveLights=0;bool lightsDirty=true,directionalOnly=false;
 void RebuildLightArray();
};
'''
tests=r'''
int main(){
 unsigned prepared=0,general=0;
 for(unsigned test=0;test<24000;++test){
  Reference::picaContext a;Optimized::picaContext b;
  bool eligible=true;unsigned active=0;
  for(unsigned i=0;i<4;++i){
   pddiLight light;light.enabled=(test>>i)&1;light.colour.c=(test*301+i*135)*65537;
   light.attenA=test%3?.9f:0;light.attenB=test%3?.1f:0;light.attenC=test%3?.01f:0;
   C3D_FVec eye;eye.x=.2f+float((test*7+i)%100);eye.y=.1f+float((test*11+i)%100);eye.z=.3f+float((test*13+i)%100);
   const float len=std::sqrt(eye.x*eye.x+eye.y*eye.y+eye.z*eye.z);eye.x/=len;eye.y/=len;eye.z/=len;
   if(test%5==0&&i==2)eye.w=1;
   if(test%7==0&&i==1)eye.x=eye.y=eye.z=0;
   if(test%11==0&&i==0)eye.x=eye.y=eye.z=2;
   if(light.enabled){++active;C3D_FVec value;eligible=eligible&&picaDirectionalLight::Prepare(eye,value);}
   a.lighting.light[i]=b.lighting.light[i]=light;a.eyeLight[i]=b.eyeLight[i]=eye;
  }
  a.RebuildLightArray();b.RebuildLightArray();
  assert(a.nActiveLights==b.nActiveLights&&b.nActiveLights==int(active));
  assert(!memcmp(a.lightPos,b.lightPos,sizeof(a.lightPos)));
  assert(!memcmp(a.lightColour,b.lightColour,sizeof(a.lightColour)));
  assert(!memcmp(a.lightAtten,b.lightAtten,sizeof(a.lightAtten)));
  assert(!b.lightsDirty&&b.directionalOnly==(eligible&&active>0));
  if(b.directionalOnly){++prepared;for(unsigned i=0;i<active;++i)assert(!memcmp(&b.directionalPos[i],&b.lightPos[i],sizeof(C3D_FVec)));}
  else ++general;
 }
 printf("PASS: 24000 production light-array builds; %u directional sets, %u general/empty/degenerate sets; original arrays and order exact\n",prepared,general);
}
'''
code=prelude+'namespace Reference {\n'+controller+method+'\n}\n#define SRR2_OLD3DS_DIRECTIONAL_LIGHT\nnamespace Optimized {\n'+controller+method+'\n}\n'+tests
with tempfile.TemporaryDirectory(prefix='srr-directional-backend-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
