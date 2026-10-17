#!/usr/bin/env python3
"""Run the production uniform writer against a sentinel-filled 24-bone palette."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
source = (root/'libs/pure3d/pddi/pica/context.cpp').read_text()
a = source.index('void picaContext::SetVertexUniforms(')
b = source.index('\nvoid picaContext::EnableCulling', a)
method = source[a:b]
harness = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#define SRR2_OLD3DS_GPU_SKIN_COMPACT
#define SRR2_OLD3DS_LIGHT_LOOP
#define SRR2_OLD_3DS
struct C3D_FVec {float x=0,y=0,z=0,w=0;};
struct C3D_Mtx {C3D_FVec r[4];};
struct pddiMatrix {float m[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
 const float* Row4(int i)const{return m[i];}};
struct pddiColour {unsigned c=0;unsigned Red()const{return c&255;}unsigned Green()const{return(c>>8)&255;}
 unsigned Blue()const{return(c>>16)&255;}unsigned Alpha()const{return c>>24;}};
enum {GPU_VERTEX_SHADER,PDDI_MATRIX_MODELVIEW=0,O3_UNIFORMS,O3_MV_UPLOAD,O3_MV_REUSE};
enum {VSH_FVEC_u_proj=0,VSH_FVEC_u_mv=4,VSH_FVEC_u_lightPos=8,VSH_FVEC_u_lightColour=12,
 VSH_FVEC_u_lightAtten=16,VSH_FVEC_u_matAmbient=20,VSH_FVEC_u_matDiffuse=21,
 VSH_FVEC_u_matEmissive=22,VSH_FVEC_u_sceneAmbient=23,VSH_FVEC_u_misc=24,
 VSH_FVEC_u_stereoOn=26,VSH_IVEC_u_lightLoop=0,VSH_FLAG_u_hasLights=1};
struct Old3dsOperationScope {Old3dsOperationScope(int,int){}};
void Old3dsCount(int){}
unsigned g_old3dsModelViewUploads=0,g_old3dsModelViewReuses=0;
bool g_picaInFrame=true;
C3D_FVec uniforms[96];bool booleans[16];unsigned writes[96];
void C3D_FVUnifSet(int,unsigned i,float x,float y,float z,float w){assert(i<96);++writes[i];uniforms[i]={x,y,z,w};}
C3D_FVec* C3D_FVUnifWritePtr(int,unsigned i,int n){assert(n>=0&&i+n<=96);for(int j=0;j<n;++j)++writes[i+j];return uniforms+i;}
void C3D_FVUnifMtx4x4(int,unsigned i,const C3D_Mtx* m){assert(i+4<=96);for(unsigned j=0;j<4;++j){++writes[i+j];uniforms[i+j]=m->r[j];}}
void C3D_BoolUnifSet(int,unsigned i,bool b){assert(i>=0x68&&i<0x78);booleans[i-0x68]=b;}
void C3D_IVUnifSet(int,unsigned,int,int,int,int){}
struct Stack {pddiMatrix matrix;pddiMatrix* Top(){return &matrix;}};
struct Lighting {pddiColour ambient;};
struct State {Stack stack;Stack* matrixStack[1]={&stack};Lighting lighting;Lighting* lightingState=&lighting;};
struct picaContext {
 State state;C3D_Mtx hwProjection;
 bool skinProgramBound=true,stereoOn=false,projUploaded=false,miscUploaded=false,lastLit=false,
 lightsDirty=true,lightsUploaded=false,matUploaded=false,modelViewUploaded=false;
 pddiMatrix lastModelView;pddiColour lastMatAmbient,lastMatDiffuse,lastMatEmissive,lastSceneAmbient;
 C3D_FVec lightPos[4],lightColour[4],lightAtten[4];int nActiveLights=4;
 void InvalidateUniformCache(){projUploaded=miscUploaded=lightsUploaded=matUploaded=modelViewUploaded=false;}
 void RebuildLightArray(){lightsDirty=false;}
 void SetVertexUniforms(bool,pddiColour,pddiColour,pddiColour,const float(*)[4]);
};
'''
tests = r'''
int main(){
 picaContext c;pddiColour a,d,e;a.c=0x117f4931;d.c=0x8f432198;e.c=0xfedcba98;
 for(unsigned i=24;i<96;++i)uniforms[i]={float(i),float(i+1),float(i+2),float(i+3)};
 C3D_FVec palette[72];memcpy(palette,uniforms+24,sizeof(palette));
 for(unsigned n=0;n<10000;++n){
  bool lit=n%4!=0;
  if(n%7==0)c.InvalidateUniformCache();
  c.state.stack.matrix.m[3][0]=float(n);
  c.state.lighting.ambient.c=n*65537;
  c.nActiveLights=n%5;c.lightsDirty=true;
  float prepared[3][4]={{float(a.Red())/255,float(a.Green())/255,float(a.Blue())/255,1},
   {float(d.Red())/255,float(d.Green())/255,float(d.Blue())/255,float(d.Alpha())/255},
   {float(e.Red())/255,float(e.Green())/255,float(e.Blue())/255,0}};
  c.SetVertexUniforms(lit,a,d,e,n%2?prepared:nullptr);
  assert(!memcmp(palette,uniforms+24,sizeof(palette)));
  assert(booleans[1]==lit&&uniforms[20].w==1&&uniforms[22].w==0&&uniforms[23].w==1.0f/255);
  if(lit){assert(uniforms[20].x==float(a.Red())/255&&uniforms[21].w==float(d.Alpha())/255);
   assert(uniforms[23].x==float(c.state.lighting.ambient.Red())/255);assert(booleans[0]==(c.nActiveLights!=0));}
  a.c+=7;d.c+=13;e.c+=19;
 }
 for(unsigned i=24;i<96;++i)assert(writes[i]==0);
 // Returning to the normal program must restore its own misc/stereo constants.
 c.skinProgramBound=false;c.InvalidateUniformCache();c.SetVertexUniforms(true,a,d,e,nullptr);
 assert(writes[24]&&writes[26]&&uniforms[24].x==1&&uniforms[23].w==1);
 g_picaInFrame=false;c.SetVertexUniforms(true,a,d,e,nullptr);assert(!c.matUploaded&&!c.projUploaded);
 puts("PASS: 10,000 production compact uniform writes; all 72 bone registers intact; lit/unlit, prepared/live colors, 0..4 lights, invalidation and normal-program restoration");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-compact-uniforms-') as td:
    p=Path(td)
    (p/'test.cpp').write_text(harness+method+tests)
    subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
