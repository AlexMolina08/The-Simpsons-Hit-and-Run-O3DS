#!/usr/bin/env python3
"""Check settling-only refinement, driving exclusions and elapsed-time conservation."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'code/worldsim/worldphysicsmanager.cpp').read_text()
a=source.index('    if((stepLimit == 25 && timeDeltaMilliSeconds > stepLimit)')
b=source.index('    mLoopTime = dt;',a)
schedule=source[a:b]
program=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>
#include "HEADER"
#define rReleasePrintf(...) ((void)0)
struct Result {unsigned steps;float step,elapsed;};
Result Schedule(unsigned timeDeltaMilliSeconds,unsigned mLastTime,unsigned stepLimit){
 unsigned numSubsteps=1;
 SCHEDULE
 return {numSubsteps,substep,dt};
}
int main(){
 unsigned cases=0;
 for(unsigned accepted:{35u,40u,50u}){
  assert(Old3dsSuspension::StepLimit(accepted,0,0,0,1,0)==25);
  // The original larger step remains for throttle, normal driving/coasting,
  // distant cars, steep/flipped cars and invalid state.
  for(float v:{2.f,5.f,32.f,100.f})for(float sign:{-1.f,1.f}){
   assert(Old3dsSuspension::StepLimit(accepted,0,v*sign,0,1,0)==accepted);
   assert(Old3dsSuspension::StepLimit(accepted,0,0,v*sign,1,0)==accepted);
  }
  for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
   assert(Old3dsSuspension::StepLimit(accepted,bad,0,0,1,0)==accepted);
   assert(Old3dsSuspension::StepLimit(accepted,0,bad,0,1,0)==accepted);
   assert(Old3dsSuspension::StepLimit(accepted,0,0,bad,1,0)==accepted);
   assert(Old3dsSuspension::StepLimit(accepted,0,0,0,bad,0)==accepted);
   assert(Old3dsSuspension::StepLimit(accepted,0,0,0,1,bad)==accepted);
  }
  assert(Old3dsSuspension::StepLimit(accepted,.1f,0,0,1,0)==accepted);
  assert(Old3dsSuspension::StepLimit(accepted,0,0,0,.9f,0)==accepted);
  assert(Old3dsSuspension::StepLimit(accepted,0,0,0,1,2501)==accepted);
 }
 for(unsigned limit:{25u,35u,40u,50u})for(unsigned ms=0;ms<=1000;++ms)for(unsigned last=0;last<=1000;last+=7){
  const auto r=Schedule(ms,last,limit);
  assert(r.steps>=1 && r.steps<=10);
  assert(std::fabs(double(r.step)*r.steps-double(ms)*.001)<.000001);
  assert(std::fabs(double(r.elapsed)-double(ms)*.001)<.000001);
  if(limit==25 && ms<=250)assert(r.step<=.025001f);
  // Preserve the reference hysteresis for ordinary frames outside settling.
  if(limit!=25 && ms<=250){
   unsigned expected=1;
   if((last>limit&&ms>limit)||ms>50)expected=(ms+limit-1)/limit;
   assert(r.steps==expected);
  }
  ++cases;
 }
 // One isolated 33 ms frame must also remain stable in the settling policy.
 const auto isolated=Schedule(33,16,25);assert(isolated.steps==2);
 puts("PASS: settling policy excludes normal driving and invalid states; nearby idle refinement and isolated 33 ms frames are bounded.");
 printf("PASS: %u actual scheduling cases preserve all elapsed time, cap work at ten steps and retain ordinary-frame hysteresis.\n",cases);
}
'''.replace('HEADER',str(root/'code/main/old3dssuspension.h')).replace('SCHEDULE',schedule)
with tempfile.TemporaryDirectory(prefix='srr-suspension-') as temporary:
 p=Path(temporary);(p/'test.cpp').write_text(program)
 subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-unused-but-set-parameter','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
