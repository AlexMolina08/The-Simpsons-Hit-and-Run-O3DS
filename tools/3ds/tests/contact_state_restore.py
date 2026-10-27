#!/usr/bin/env python3
"""Check restoration of normal-program state after shadow uniform uploads."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/pure3d/pddi/pica/context.cpp').read_text()

def method(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

shadow = method('void picaContext::DrawContactShadows()')
after_draw = shadow.split('C3D_DrawElements(', 1)[1]
assert 'BindSkinProgram(false);' in after_draw
assert 'C3D_BindProgram(MainProgram())' not in after_draw
shader = (root / 'libs/pure3d/pddi/pica/shaders/pddi_unlit_projection.v.pica').read_text()
offset = 0
for name, size in re.findall(r'^\.fvec\s+(\w+)(?:\[(\d+)\])?', shader, re.M):
    if name == 'u_eye':
        break
    offset += int(size or 1)
assert offset == 25

harness = r'''
#include <cassert>
#include <cstdio>
struct shaderProgram_s{};
struct Vec {float x,y,z,w;};
Vec uniforms[96];shaderProgram_s* current;
enum {GPU_VERTEX_SHADER,VSH_FVEC_u_eye=25};
const float PICA_STEREO_CONVERGENCE=3.0f;
void C3D_BindProgram(shaderProgram_s* program){current=program;}
void C3D_SetAttrInfo(int*){}
void C3D_FVUnifSet(int,unsigned index,float x,float y,float z,float w){uniforms[index]={x,y,z,w};}
struct {bool bufInfoSet=true;} g_picaState;
struct {unsigned switches=0;} g_picaSkinMetrics;
struct picaContext {
    enum {SurfaceSkin,SurfaceUnknown};
    bool skinProgramBound=false,noNormalLayout=true,invalidated=false;
    unsigned skinBatchDepth=0;
    unsigned char reflectionState=SurfaceUnknown;
    int attrInfo=0,skinAttrInfo=1;shaderProgram_s main,skin;
    shaderProgram_s* MainProgram(){return &main;}
    shaderProgram_s* SkinProgram(){return &skin;}
    void InvalidateUniformCache(){invalidated=true;}
    bool BindSkinProgram(bool);
};
'''
tests = r'''
int main(){
    picaContext context;
    for(unsigned count=0;count<=12;++count){
        uniforms[25]={0,3,0,0};
        for(unsigned i=0;i<count*3;++i)uniforms[8+i]={.7f,0,1,2};
        // The former raw program bind leaves c25 overwritten at six casters.
        C3D_BindProgram(context.MainProgram());
        assert((uniforms[25].x!=0)==(count>=6));
        context.invalidated=false;context.noNormalLayout=true;
        assert(context.BindSkinProgram(false));
        assert(current==context.MainProgram() && !context.skinProgramBound);
        assert(!context.noNormalLayout && context.invalidated && !g_picaState.bufInfoSet);
        assert(uniforms[25].x==0 && uniforms[25].y==3 && uniforms[25].z==0 && uniforms[25].w==0);
    }
    puts("PASS: actual shadow exit uses complete program restore; production restore repairs c25 after 0-12 caster uploads");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-contact-state-') as temporary:
    folder = Path(temporary)
    (folder / 'test.cpp').write_text(harness + method('bool picaContext::BindSkinProgram(bool enabled)') + tests)
    for batch in (False, True):
        flags = ['-DSRR2_OLD3DS_GPU_SKIN_BATCH'] if batch else []
        subprocess.run(['clang++', '-std=c++14', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-DSRR2_OLD3DS_GPU_SKIN_COMPACT',
                        '-DSRR2_OLD3DS_VISUAL_UPGRADE', '-DSRR2_OLD3DS_NO_NORMAL_FETCH', *flags,
                        str(folder / 'test.cpp'), '-o', str(folder / 'test')], check=True)
        subprocess.run([str(folder / 'test')], check=True)
