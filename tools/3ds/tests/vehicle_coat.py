#!/usr/bin/env python3
"""Exercise the production material setter: loading-only paint calibration."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
text=(root/'libs/pure3d/pddi/pica/material.cpp').read_text()
start=text.index('void picaMat::SetEnvironmentBlend(')
method=text[start:text.index('bool picaMat::WorldLightmapEnabled()',start)]
fixture=r'''
#define SRR2_OLD3DS_VISUAL_UPGRADE
#define SRR2_OLD3DS_NATIVE_BENCH
#include <pddi/pica/visualstyle.hpp>
#include <cassert>
#include <cstdint>
namespace picaVisual { unsigned features=0,reflectionDraws=0,sceneryDraws=0; }
struct pddiColour {
 unsigned c;
 pddiColour(unsigned r,unsigned g,unsigned b,unsigned a=255):c(r|(g<<8)|(b<<16)|(a<<24)){}
 unsigned Red()const{return c&255;}unsigned Green()const{return (c>>8)&255;}
 unsigned Blue()const{return (c>>16)&255;}unsigned Alpha()const{return c>>24;}
};
struct picaMat {
 bool vehicleSurface=false,glassSurface=false,dirty=false;
 pddiColour environmentBlend{0,0,0,0};
 void SetEnvironmentBlend(pddiColour);
};
METHOD
int main(){
 unsigned previous=0;
 for(unsigned strength=0;strength<256;++strength){
  const unsigned lifted=picaVisual::CoatReflectivity(strength);
  assert(lifted>=strength && lifted>=previous && lifted<=255);
  assert(strength>=96?lifted==strength:lifted<=96);previous=lifted;
  for(unsigned variant=0;variant<8;++variant)for(unsigned alpha:{0u,73u,255u}){
   picaMat material;material.vehicleSurface=(variant&1)!=0;material.glassSurface=(variant&2)!=0;
   picaVisual::features=(variant&4)?picaVisual::KeyLighting:0;
   const pddiColour original(strength,strength,strength,alpha);
   material.SetEnvironmentBlend(original);
   const bool active=(variant&1) && !(variant&2) && (variant&4);
   assert(material.environmentBlend.Red()==(active?lifted:strength));
   assert(material.environmentBlend.Alpha()==alpha);
   if(!active)assert(material.environmentBlend.c==original.c);
   material.dirty=false;material.SetEnvironmentBlend(original);assert(!material.dirty);
  }
 }
 unsigned random=57;
 for(unsigned i=0;i<100000;++i){
  random=random*1664525u+1013904223u;
  const pddiColour colour(random&255,(random>>8)&255,(random>>16)&255,random>>24);
  if(colour.Red()==colour.Green() && colour.Red()==colour.Blue())continue;
  picaMat material;material.vehicleSurface=true;picaVisual::features=picaVisual::KeyLighting;
  material.SetEnvironmentBlend(colour);assert(material.environmentBlend.c==colour.c);
 }
}
'''.replace('METHOD',method).replace('#include <cstdint>','#include <cstdint>\n#include <initializer_list>')
with tempfile.TemporaryDirectory(prefix='srr-vehicle-coat-') as name:
    p=Path(name);(p/'test.cpp').write_text(fixture)
    subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
        '-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: all 256 neutral strengths; strong/zero strengths, coloured materials, glass, disabled path, exact alpha and repeated-value dirty suppression. No per-frame walk added.')
