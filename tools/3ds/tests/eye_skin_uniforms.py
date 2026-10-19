#!/usr/bin/env python3
"""Protect the 25-bone eye-space palette from production material/light writes."""
from pathlib import Path
import runpy
import subprocess
import tempfile,sys
packed="--packed" in sys.argv
root=Path(__file__).resolve().parents[3]
base=runpy.run_path(str(root/'tools/3ds/tests/compact_skin_uniforms.py'))
harness=base['harness'].replace('#define SRR2_OLD3DS_GPU_SKIN_COMPACT','#define SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE\n#define SRR2_OLD3DS_GPU_SKIN_COMPACT')
tests=base['tests']
a,b=tests.split(' // Returning to the normal program',1)
a=a.replace('i=24;i<96','i=20;i<95').replace('palette[72]','palette[75]').replace('uniforms+24','uniforms+20')
for old,new in [(20,16),(21,17),(22,18),(23,19)]:
 a=a.replace('uniforms['+str(old)+']','uniforms['+str(new)+']')
tests=a+' // Returning to the normal program'+b
if packed:
 harness='#define SRR2_OLD3DS_GPU_SKIN_PACKED\n'+harness.replace('VSH_FVEC_u_proj=0','VSH_FVEC_u_eye_quant=95,VSH_FVEC_u_proj=0')
 tests=tests.replace('  assert(!memcmp(palette,uniforms+20,sizeof(palette)));','  assert(!memcmp(palette,uniforms+20,sizeof(palette)));\n  assert(uniforms[95].x==1.f/8192&&uniforms[95].y==1.f/32768&&uniforms[95].z==1.f/4096&&uniforms[95].w==1);')
 tests=tests.replace(' // Returning to the normal program',' assert(writes[95]==10000);\n // Returning to the normal program')
 tests=tests.replace(' g_picaInFrame=false;',' assert(writes[95]==10000);\n g_picaInFrame=false;')

if '--directional' in sys.argv:
 harness='#define SRR2_OLD3DS_DIRECTIONAL_LIGHT\n'+harness
 harness=harness.replace('VSH_FVEC_u_proj=0','VSH_FLAG_u_directionalOnly=4,VSH_FVEC_u_proj=0')
 harness=harness.replace(' State state;C3D_Mtx hwProjection;', ' bool directionalOnly=false;unsigned directionalDraws=0,generalLightDraws=0;C3D_FVec directionalPos[4];\n State state;C3D_Mtx hwProjection;')
 tests=tests.replace('  bool lit=n%4!=0;', '  bool lit=n%4!=0;c.directionalOnly=n%2!=0;\n  for(unsigned i=0;i<4;++i){c.lightPos[i]={float(n+i),2,3,1};c.directionalPos[i]={4,float(n+i),6,0};}')
 tests=tests.replace('  a.c+=7;d.c+=13;e.c+=19;', '  if(lit){assert(booleans[2]==c.directionalOnly);for(int i=0;i<c.nActiveLights;++i){const C3D_FVec expected=c.directionalOnly?c.directionalPos[i]:c.lightPos[i];assert(!memcmp(&expected,uniforms+4+i,sizeof(expected)));}}\n  a.c+=7;d.c+=13;e.c+=19;')

if '--unlit' in sys.argv:
 harness='#include <pddi/pica/unlitprojection.hpp>\n#define SRR2_OLD3DS_UNLIT_PROJECTION\n#define SRR2_OLD3DS_GPU_SKIN_SUPPORT\n'+harness
 harness=harness.replace('VSH_FVEC_u_proj=0','VSH_FLAG_u_fastUnlit=2,VSH_FVEC_u_unlitScale=27,VSH_FVEC_u_proj=0')
 harness=harness.replace('struct picaContext {','struct TestDisplay {bool active=false;bool StereoActive()const{return active;}};\nstruct picaContext {TestDisplay testDisplay;TestDisplay* display=&testDisplay;')
 harness=harness.replace(' State state;C3D_Mtx hwProjection;', ' unsigned fastUnlitCalls=0,otherProjectionCalls=0;bool sparseUnlitProjection=false,lastFastUnlit=false;\n State state;C3D_Mtx hwProjection;')
 extra=r''' // Exercise the actual normal-program fast-path guards and sparse uploads.
 c.hwProjection={};c.hwProjection.r[0].y=-1.5f;c.hwProjection.r[1].x=2;
 c.hwProjection.r[2].z=-1.001f;c.hwProjection.r[2].w=-.1f;c.hwProjection.r[3].z=-1;
 for(unsigned n=0;n<10000;++n){
  c.skinProgramBound=false;c.stereoOn=true;c.testDisplay.active=n%5==1;
  c.state.stack.matrix.m[0][3]=n%5==2?.2f:0;
  c.hwProjection.r[0].w=n%5==3?.1f:0;
  bool lit=n%5==4;c.InvalidateUniformCache();c.SetVertexUniforms(lit,a,d,e,nullptr);
  assert(booleans[1]==(n%5==0));
  if(n%5==0)assert(uniforms[27].x==-1.5f&&uniforms[27].y==2);
 }
'''
 tests=tests.replace(' g_picaInFrame=false;',extra+' g_picaInFrame=false;')

with tempfile.TemporaryDirectory(prefix='srr-eye-uniforms-') as td:
 p=Path(td);(p/'test.cpp').write_text(harness+base['method']+tests)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: eye-space production writer protects c20..c94 and restores the normal shader layout')
