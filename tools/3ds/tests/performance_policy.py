#!/usr/bin/env python3
"""Check policy isolation and actual substep selection against the frozen engine."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
name='code/worldsim/worldphysicsmanager.cpp'
after=(root/name).read_text()
def steps(source):
 start=source.index('    unsigned numSubsteps = 1;',source.index('void WorldPhysicsManager::Update('))
 end=source.index('#ifdef SRR2_OLD_3DS',start)
 return source[start:end]+'return numSubsteps;\n'
source='''#include <cassert>
#include <main/old3dsperformance.h>
unsigned Old3dsPerformance::mode=0;
unsigned reference(unsigned mLastTime,unsigned timeDeltaMilliSeconds){
    // Frozen 35 ms rule from 10731fb, independent of the current Git checkout.
    unsigned numSubsteps = 1;
    if(((mLastTime > 35) && (timeDeltaMilliSeconds > 35)) || (timeDeltaMilliSeconds > 50))
        numSubsteps = (timeDeltaMilliSeconds + 34) / 35;
    return numSubsteps;
}
unsigned candidate(unsigned mLastTime,unsigned timeDeltaMilliSeconds){
'''+steps(after)+'''}
int main(){
 for(unsigned mode=0;mode<8;++mode){
  Old3dsPerformance::mode=mode;
  for(unsigned previous=0;previous<=1000;++previous)
   for(unsigned elapsed=0;elapsed<=1000;++elapsed){
    unsigned original=reference(previous,elapsed),actual=candidate(previous,elapsed);
    assert(actual>0);
    if(mode<2)assert(original==actual);
    else{
     assert(actual<=original);
     unsigned limit=Old3dsPerformance::PhysicsStepMilliseconds();
     if(elapsed>50 || (elapsed>limit && previous>limit))assert(double(elapsed)/actual<=double(limit));
    }
   }
 }
}
'''
with tempfile.TemporaryDirectory(prefix='srr-performance-policy-') as d:
 p=Path(d);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O2','-DSRR2_OLD_3DS','-fsanitize=address,undefined','-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: 8,016,008 substep decisions; reference/moderate tiers match frozen engine, performance tiers preserve long-stall subdivision.')
