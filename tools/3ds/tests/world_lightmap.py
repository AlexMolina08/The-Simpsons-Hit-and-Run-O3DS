#!/usr/bin/env python3
"""Exercise production lightmap uniform caching across skin palette overwrites."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
def method(signature):
    begin=source.index(signature);opening=source.index('{',begin);end=opening+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[begin:end]
fixture=r'''
#define SRR2_OLD3DS_VISUAL_UPGRADE
#define SRR2_OLD_3DS
#include <cassert>
#include <cstring>
struct picaTexture {float p[4];const float* WorldLightmapParameters(){return p;}};
struct {const picaTexture* worldLightmapParameters=nullptr;} g_picaState;
enum {GPU_VERTEX_SHADER,VSH_FVEC_u_lightPos=8};
float uniform[4];unsigned uploads=0;
void C3D_FVUnifSet(int,int,float a,float b,float c,float d){float p[]={a,b,c,d};memcpy(uniform,p,sizeof(p));++uploads;}
struct picaContext {
 enum {SurfaceUnknown,SurfaceSkin};bool skinProgramBound=false;unsigned reflectionState=0;
 bool modelViewUploaded=false,projUploaded=false,lightsUploaded=false,matUploaded=false,miscUploaded=false;
 void SetWorldLightmapParameters(picaTexture*);
 void InvalidateUniformCache();
};
METHODS
int main(){picaContext c;picaTexture a{{.001f,.002f,.3f,.4f}},b{{.005f,.006f,.7f,.8f}};
 unsigned random=19;
 for(unsigned i=0;i<100000;++i){random=random*1664525u+1013904223u;
  picaTexture* selected=(random&1)?&a:&b;
  if(random%3==0){for(auto& f:uniform)f=-99;c.skinProgramBound=!c.skinProgramBound;c.InvalidateUniformCache();assert(!g_picaState.worldLightmapParameters);}
  const unsigned before=uploads;const bool cached=g_picaState.worldLightmapParameters==selected;
  c.SetWorldLightmapParameters(selected);assert(uploads==before+!cached);assert(!memcmp(uniform,selected->p,sizeof(uniform)));
  assert(!c.lightsUploaded);
 }
}
'''.replace('METHODS',method('void picaContext::SetWorldLightmapParameters(')+'\n'+method('void picaContext::InvalidateUniformCache('))
with tempfile.TemporaryDirectory(prefix='srr-world-lightmap-') as name:
    p=Path(name);(p/'test.cpp').write_text(fixture)
    subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: 100000 production lightmap cache transitions; skin palette overwrites force restoration, unchanged maps skip uploads, lit uniforms remain invalidated.')
