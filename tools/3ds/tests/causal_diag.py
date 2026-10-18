#!/usr/bin/env python3
"""Exercise production diagnostic windows, exclusive scopes and frame attribution."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'main').mkdir()
 (p/'3ds.h').write_text('''#pragma once
#include <cstdint>
using u64=unsigned long long;using u32=unsigned;using ThreadFunc=void(*)(void*);
const unsigned SYSCLOCK_ARM11=1000;static u64 ticks=0;
inline u64 svcGetSystemTick(){return ticks+=10;}
inline unsigned linearSpaceFree(){return 4000000;}
enum GSPGPU_Event{GSPGPU_EVENT_VBlank0};enum gfxScreen_t{GFX_TOP,GFX_BOTTOM};
''')
 (p/'citro3d.h').write_text('inline float C3D_GetDrawingTime(){return 7.0f;}\n')
 (p/'main/old3dsdiag.h').write_text('''#include <3ds.h>
const unsigned O3_CATEGORY_COUNT=2;struct Entity{unsigned calls;};
Entity g_old3dsEntities[2]={};unsigned g_old3dsPhysicsSubsteps=0,g_old3dsCollisionPairs=0,g_old3dsPhysicsAreas=0;
''')
 code='''#include <cassert>
#include <main/old3dsexperiment.cpp>
extern "C" void __real_gspSetEventCallback(GSPGPU_Event,ThreadFunc,void*,bool){}
extern "C" void __real_gfxScreenSwapBuffers(gfxScreen_t,bool){}
int main(){
 O3ExperimentArm(false);unsigned expected[]={0,1,0,2,0,3,0,4,0};
 for(unsigned f=0;f<810;++f){
  O3ExperimentBegin(f*3,true);assert(g_o3Mode==expected[f/90]);
  if(f==0){
   {O3ExperimentTimer outer(O3E_BACKEND);{O3ExperimentTimer inner(O3E_SUBMIT);}}
   assert(current.inclusive[O3E_SUBMIT]==10);
   assert(current.inclusive[O3E_BACKEND]==30 && current.exclusive[O3E_BACKEND]==20);
   O3ExperimentSubmit();__wrap_gfxScreenSwapBuffers(GFX_TOP,false);onVblank(nullptr);
   assert(completions[1].id==1 && completions[1].visible>completions[1].swap);
  }
  O3ExperimentEnd(100,42);
 }
 assert(count==810 && !armed && !frames[0].fixed);
 O3ExperimentBegin(9000,false);assert(g_o3Mode==0);
 O3ExperimentArm(true);O3ExperimentBegin(9999,true);assert(O3ExperimentFixed());
 O3ExperimentSubmit();__wrap_gfxScreenSwapBuffers(GFX_TOP,false);onVblank(nullptr);
 O3ExperimentEnd(100,50);
 assert(frames[810].fixed && !frames[0].fixed);
 assert(completions[811].id==811 && completions[811].gpu==7.0f);
 O3ExperimentBegin(10000,true);
 {O3ExperimentTimer physics(O3E_PHYSICS);for(int i=0;i<3;++i){O3ExperimentTimer step(O3E_SUBSTEP);}}
 assert(current.step[0] && current.step[1] && current.step[2] && !current.step[3]);
 O3ExperimentEnd(100,50);
 // Automated routes own capture lifetime and hold one intervention per boot.
 O3ExperimentExternal(O3_COVERAGE,false);
 O3ExperimentBegin(11000,true);assert(g_o3Mode==O3_REFERENCE);
 O3ExperimentExternalCapture(true);
 for(unsigned f=0;f<900;++f){
  O3ExperimentBegin(12000+f,true);assert(g_o3Mode==O3_COVERAGE);
  assert(!O3ExperimentFixed());O3ExperimentEnd(100,42);
 }
 O3ExperimentArm(true);assert(!O3ExperimentFixed());
 O3ExperimentExternalCapture(false);O3ExperimentBegin(14000,true);
 assert(g_o3Mode==O3_REFERENCE);
}
'''
 (p/'test.cpp').write_text(code)
 subprocess.run(['c++','-std=c++17','-DSRR2_OLD3DS_CAUSAL_DIAG','-fsanitize=address,undefined','-I'+str(p),'-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: production windows, nested exclusivity, substeps, fixed/adaptive labels and sparse-frame GPU/presentation attribution')
