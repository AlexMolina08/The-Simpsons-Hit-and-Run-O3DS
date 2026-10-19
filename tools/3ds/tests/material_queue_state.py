#!/usr/bin/env python3
"""Execute production scoped PICA pipeline compatibility and selective binding."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/pddi/pica/material.cpp').read_text()
def function(signature):
    start=source.index(signature); first=source.index('{',start); depth=1; end=first+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}'); end+=1
    return source[start:end]
harness=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
struct C3D_Tex { unsigned id; int filter,wrap; };
struct picaTexture { C3D_Tex texture; bool valid=true; C3D_Tex* GetC3DTexture() { return valid ? &texture : nullptr; } };
struct pddiColour { unsigned c; };
struct picaTextureEnv {
    picaTexture* texture=nullptr;
    int filterMode=0,uvMode=0,alphaBlendMode=0,alphaCompareMode=0,writeMask=15;
    bool alphaTest=false,twoSided=false,lit=false;
    float alphaRef=0.5f;
    pddiColour ambient={0},diffuse={0},emissive={0};
};
struct picaContext {
    unsigned uniforms=0;
    void SetVertexUniforms(bool,pddiColour,pddiColour,pddiColour) { ++uniforms; }
    void ADD_STAT(unsigned,bool) {}
};
struct picaState {
    bool alphaTestValid=true,blendValid=true,cullValid=true,depthValid=true,passStagesSet=true;
    int env0Mode=1;
} g_picaState;
bool g_picaInFrame=true,g_old3dsDrawBlended=false,g_old3dsDrawAlphaTested=false;
enum { O3_MATERIAL, PDDI_STAT_MATERIAL_OPS, PDDI_BLEND_NONE=0 };
struct Old3dsOperationScope { explicit Old3dsOperationScope(int) {} };
const int filterTable[]={10,20}, uvTable[]={30,40};
C3D_Tex* bound=nullptr;
unsigned binds=0;
void C3D_TexSetFilter(C3D_Tex* t,int a,int b) { assert(a==b); t->filter=a; }
void C3D_TexSetWrap(C3D_Tex* t,int a,int b) { assert(a==b); t->wrap=a; }
void picaTexBind0(C3D_Tex* t) { bound=t; ++binds; }
struct picaMat {
    picaContext* context;
    picaTextureEnv texEnv[1];
    unsigned uid;
    bool dirty=true;
    static picaMat* lastShader;
    static unsigned currentUID;
    bool IsCurrent() const { return currentUID==uid; }
    bool CanApplyQueued(picaMat*) const;
    void ApplyQueued(picaMat*);
};
picaMat* picaMat::lastShader=nullptr;
unsigned picaMat::currentUID=0;
'''
tests=r'''
int main() {
    picaContext context,foreign;
    picaTexture first,second; first.texture={1,10,30}; second.texture={2,10,30};
    picaMat previous,next; previous.context=next.context=&context; previous.uid=1; next.uid=2;
    previous.texEnv[0].texture=&first; next.texEnv[0].texture=&second;
    picaMat::lastShader=&previous; picaMat::currentUID=previous.uid;
    assert(next.CanApplyQueued(&previous));
    next.ApplyQueued(&previous);
    assert(bound==&second.texture && binds==1 && !next.dirty && context.uniforms==0);
    assert(picaMat::lastShader==&next && picaMat::currentUID==2);
    // Identical material keeps the exact texture/sampler and common uniforms.
    assert(next.CanApplyQueued(&next)); next.ApplyQueued(&next);
    assert(binds==1 && context.uniforms==0);
    // Different lighting/color data must refresh vertex uniforms.
    next.texEnv[0].lit=true; next.ApplyQueued(&previous); assert(context.uniforms==1);
    previous.texEnv[0].lit=true;
    next.ApplyQueued(&previous); assert(context.uniforms==1);
    next.texEnv[0].diffuse.c=123;
    next.ApplyQueued(&previous); assert(context.uniforms==2);
    // Sampler-only changes must retain their exact modes.
    next.texEnv[0].filterMode=next.texEnv[0].uvMode=1;
    next.ApplyQueued(&previous);
    assert(second.texture.filter==20 && second.texture.wrap==40);
    picaMat::lastShader=&previous; picaMat::currentUID=previous.uid;
#define REJECT(field,value) { const auto saved=next.texEnv[0].field; next.texEnv[0].field=value; assert(!next.CanApplyQueued(&previous)); next.texEnv[0].field=saved; }
    REJECT(twoSided,true); REJECT(writeMask,0); REJECT(alphaBlendMode,1); REJECT(alphaTest,true);
    previous.texEnv[0].alphaTest=next.texEnv[0].alphaTest=true;
    REJECT(alphaCompareMode,1); REJECT(alphaRef,0.4f);
    previous.texEnv[0].alphaTest=next.texEnv[0].alphaTest=false;
    REJECT(texture,nullptr);
#define INVALID(field) g_picaState.field=false; assert(!next.CanApplyQueued(&previous)); g_picaState.field=true
    INVALID(alphaTestValid); INVALID(blendValid); INVALID(cullValid); INVALID(depthValid); INVALID(passStagesSet);
    g_picaState.env0Mode=2; assert(!next.CanApplyQueued(&previous)); g_picaState.env0Mode=1;
    next.context=&foreign; assert(!next.CanApplyQueued(&previous)); next.context=&context;
    picaMat::currentUID=999; assert(!next.CanApplyQueued(&previous)); picaMat::currentUID=previous.uid;
    g_picaInFrame=false; assert(!next.CanApplyQueued(&previous));
    std::puts("PASS production queued pipeline: exact texture/sampler binding, common uniform reuse, lit/color refresh, UID handoff, alpha/blend/cull/write/texture/cache/context/frame fallback");
}
'''
functions=function('bool picaMat::CanApplyQueued(')+'\n'+function('void picaMat::ApplyQueued(')
with tempfile.TemporaryDirectory(prefix='srr2-queue-state-') as directory:
    p=Path(directory); (p/'test.cpp').write_text(harness+'\n'+functions+'\n'+tests)
    subprocess.run(['clang++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
