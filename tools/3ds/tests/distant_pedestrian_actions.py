#!/usr/bin/env python3
"""Exercise production distant-action eligibility, staggering and elapsed-time retention."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'code/worldsim/character/character.cpp').read_text()
pre=s[s.index('void Character::PreSimUpdate('):s.index('void Character::ResetCollisions')]
post=s[s.index('void Character::PostSimUpdate('):s.index('BEGIN_PROFILE("OnPostSimUpdate")',s.index('void Character::PostSimUpdate('))]
def policy(part):
 a=part.index('    const bool lowDetailActions');b=part.index('if(lowDetailActions) modulo = 0x3;',a)+len('if(lowDetailActions) modulo = 0x3;')
 return part[a:b]
a=pre.index('if(        ');a=pre.index('(',a);depth=1;b=a+1
while depth:
 if pre[b]=='(':depth+=1
 if pre[b]==')':depth-=1
 b+=1
condition=pre[a+1:b-1]
a=post.index('    bool shouldUpdate =');b=post.index(';',a)+1
fixture='''#include <cassert>
#include <cstddef>
namespace Old3dsPerformance {bool enabled=true;bool DistantPedestrianActions(){return enabled;}}
namespace sim {enum {simAICtrl,simSimulationCtrl};}
enum {ROLE_UNKNOWN,ROLE_PEDESTRIAN,ROLE_DRIVER,ROLE_MISSION};
struct Sim {int control=sim::simAICtrl;int GetControl(){return control;}};
struct Character {
 Sim state;Sim* mpSimStateObj=&state;int mRole=ROLE_PEDESTRIAN;
 bool mTooFarToUpdate=true,mbInAnyonesFrustrum=true,mbSimpleLoco=true,mIsNPC=true,inCar=false;
 void* mpTargetVehicle=nullptr;unsigned mIntersectFrame=0;
 bool IsInCar(){return inCar;}
 bool pre(unsigned frameCount){unsigned modulo=0x7;
'''+policy(pre)+'\nreturn '+condition+';\n}\n bool post(unsigned frameCount){unsigned modulo=0x7;\n'+policy(post)+'\n'+post[a:b]+'\nreturn shouldUpdate;\n}\n};\n'
main=r'''
int main(){
 for(unsigned phase=0;phase<8;++phase){
  Character c;c.mIntersectFrame=phase;unsigned preTime=0,postTime=0,prePending=0,postPending=0,preCalls=0,postCalls=0;
  for(unsigned frame=0;frame<64;++frame){
   prePending+=33;postPending+=33;
   if(c.pre(frame)){preTime+=prePending;prePending=0;++preCalls;}
   if(c.post(frame)){postTime+=postPending;postPending=0;++postCalls;}
  }
  assert(preCalls==16 && postCalls==16);
  assert(preTime+prePending==64*33 && postTime+postPending==64*33);
  c.mTooFarToUpdate=false;assert(c.pre(64)&&c.post(64));
 }
 for(unsigned scenario=0;scenario<7;++scenario){
  Character c;
  switch(scenario){
   case 0:c.mRole=ROLE_MISSION;break;
   case 1:c.mIsNPC=false;c.mRole=ROLE_UNKNOWN;break;
   case 2:c.mTooFarToUpdate=false;break;
   case 3:c.mpTargetVehicle=&c;break;
   case 4:c.state.control=sim::simSimulationCtrl;break;
   case 5:c.mbSimpleLoco=false;break;
   case 6:Old3dsPerformance::enabled=false;break;
  }
  for(unsigned frame=0;frame<64;++frame)assert(c.pre(frame)&&c.post(frame));
  Old3dsPerformance::enabled=true;
 }
 Character offscreen;offscreen.mbInAnyonesFrustrum=false;unsigned calls=0;
 for(unsigned frame=0;frame<64;++frame)calls+=offscreen.pre(frame);
 assert(calls==8);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-ped-actions-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture+main)
 subprocess.run(['clang++','-std=c++17','-O2','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: only distant visible ambient walkers are staggered; accumulated time retained; player, mission, near, driver, ragdoll and complex-animation updates remain full rate; original offscreen cadence preserved.')
