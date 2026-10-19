#!/usr/bin/env python3
"""Exercise real native bounds, context guards/cache and early submission rejection."""
from pathlib import Path
import subprocess,tempfile,sys
planes="--planes" in sys.argv
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/pddi/pica/context.cpp').read_text()
def extract(signature):
 start=source.index(signature+'\n{');opening=source.index('{',start);end=opening+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
prelude=r'''
#include <pddi/pica/clipbounds.hpp>
#include <pddi/pica/nonormal.hpp>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <limits>
#define SRR2_OLD3DS_CLIP_BOUNDS
#define SRR2_OLD3DS_GPU_SKIN_SUPPORT
#define ADD_STAT(...) ((void)0)
const unsigned PICA_VERTEX_STRIDE=36;
const int PDDI_MATRIX_MODELVIEW=0,PDDI_PROJECTION_PERSPECTIVE=1;
bool g_picaInFrame=true;
struct C3D_FVec {float w=0,z=0,y=0,x=0;};
struct C3D_Mtx {C3D_FVec r[4];};
struct pddiMatrix {float m[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};const float* Row4(unsigned i)const{return m[i];}};
struct Display {bool world=true,stereo=false;bool IsWorldTarget()const{return world;}bool StereoActive()const{return stereo;}};
struct Stack {pddiMatrix matrix;pddiMatrix* Top(){return &matrix;}};
struct View {int projectionMode=PDDI_PROJECTION_PERSPECTIVE;};
struct State {Stack stack;Stack* matrixStack[1]={&stack};View view;View* viewState=&view;};
static unsigned materialCalls,draws;
struct pddiShader {};
struct pddiBaseShader:pddiShader {bool IsCurrent(){return false;}void SetMaterial(){++materialCalls;}};
struct pddiPrimBuffer {};
struct picaContext {
 Display d;Display* display=&d;State state;C3D_Mtx hwProjection;
 bool bottomPass=false,clipPrepared=false,skinProgramBound=false;pddiMatrix clipModelView;C3D_Mtx clipProjection;
#ifdef SRR2_OLD3DS_CLIP_PLANE_PREP
 picaClipBounds::ProjectionPlanes clipProjectionPlanes;
#endif
 picaClipBounds::Transform clipTransform;unsigned clipTested=0,clipRejected=0;
 pddiBaseShader defaultMaterial;pddiShader* defaultShader=&defaultMaterial;
 bool PrepareParticleClip(picaClipBounds::Transform&) const;bool CullBounds(const float[3],const float[3]);void DrawPrimBuffer(pddiShader*,pddiPrimBuffer*);
};
bool PrimIsDrawable(unsigned p){return p<2;}
struct picaPrimBuffer:pddiPrimBuffer {
 picaContext* context;bool transient=false,noNormalAllowed=true,noNormalPacked=false,clipBoundsValid=false,skinDrawing=false;
 unsigned char* buffer;unsigned allocated=3,primType=0;float clipLow[3],clipHigh[3];
 void UpdateClipBounds();bool CullNative();void Display(){++draws;}
};
'''
methods='\n'.join(extract(sig) for sig in [
 'bool picaContext::PrepareParticleClip(picaClipBounds::Transform& output) const',
 'bool picaContext::CullBounds(const float low[3], const float high[3])',
 'void picaPrimBuffer::UpdateClipBounds()', 'bool picaPrimBuffer::CullNative()',
 'void picaContext::DrawPrimBuffer(pddiShader* mat, pddiPrimBuffer* buffer)'])
tests=r'''
int main(){
 picaContext c;c.hwProjection.r[0].x=c.hwProjection.r[1].y=c.hwProjection.r[2].z=c.hwProjection.r[3].w=1;
 picaClipBounds::Transform particle;assert(c.PrepareParticleClip(particle)&&particle.valid);
 c.skinProgramBound=true;assert(!c.PrepareParticleClip(particle)&&!particle.valid);c.skinProgramBound=false;
 g_picaInFrame=false;assert(!c.PrepareParticleClip(particle));g_picaInFrame=true;
 c.bottomPass=true;assert(!c.PrepareParticleClip(particle));c.bottomPass=false;
 c.d.stereo=true;assert(!c.PrepareParticleClip(particle));c.d.stereo=false;
 c.state.view.projectionMode=0;assert(!c.PrepareParticleClip(particle));c.state.view.projectionMode=1;
 assert(c.PrepareParticleClip(particle));
 alignas(16) unsigned char data[3*36]={};
 for(unsigned i=0;i<3;++i){float p[]={3+float(i),float(i)/10,0};memcpy(data+i*36,p,12);}
 picaPrimBuffer b;b.context=&c;b.buffer=data;b.UpdateClipBounds();assert(b.clipBoundsValid&&b.clipLow[0]==3&&b.clipHigh[0]==5);
 c.DrawPrimBuffer(nullptr,&b);assert(c.clipRejected==1&&!materialCalls&&!draws);
 c.state.stack.matrix.m[3][0]=-4;c.DrawPrimBuffer(nullptr,&b);assert(materialCalls==1&&draws==1);
 c.state.stack.matrix.m[3][0]=0;c.DrawPrimBuffer(nullptr,&b);assert(c.clipRejected==2&&draws==1);
 // Projection changes invalidate the prepared planes independently of the matrix.
 c.hwProjection.r[3].w=10;c.DrawPrimBuffer(nullptr,&b);assert(draws==2);
 c.hwProjection.r[3].w=1;
 c.d.stereo=true;c.DrawPrimBuffer(nullptr,&b);assert(draws==3);c.d.stereo=false;
 c.d.world=false;c.DrawPrimBuffer(nullptr,&b);assert(draws==3&&c.clipRejected==3);
 c.bottomPass=true;c.DrawPrimBuffer(nullptr,&b);assert(draws==4);c.bottomPass=false;
 c.state.view.projectionMode=0;c.DrawPrimBuffer(nullptr,&b);assert(draws==5);c.state.view.projectionMode=1;
 b.skinDrawing=true;c.DrawPrimBuffer(nullptr,&b);assert(draws==6);b.skinDrawing=false;
 b.transient=true;c.DrawPrimBuffer(nullptr,&b);assert(draws==7);b.transient=false;
 b.primType=99;assert(!b.CullNative());b.primType=0;
 float low[3],high[3];memcpy(low,b.clipLow,12);memcpy(high,b.clipHigh,12);
 picaNoNormal::Pack(data,3);b.noNormalPacked=true;b.UpdateClipBounds();assert(b.clipBoundsValid&&!memcmp(low,b.clipLow,12)&&!memcmp(high,b.clipHigh,12));
 float nan=std::numeric_limits<float>::quiet_NaN();memcpy(data,&nan,4);b.UpdateClipBounds();assert(!b.clipBoundsValid&&!b.CullNative());
 b.noNormalAllowed=false;b.UpdateClipBounds();assert(!b.clipBoundsValid);
 puts("PASS: native 24/36-byte bounds, matrix/projection cache invalidation, material-before-draw rejection, stereo/target/skin/transient guards and nonfinite fallback");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-clip-backend-') as td:
 p=Path(td);(p/'test.cpp').write_text(('#define SRR2_OLD3DS_CLIP_PLANE_PREP\n' if planes else '')+prelude+methods+tests)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
