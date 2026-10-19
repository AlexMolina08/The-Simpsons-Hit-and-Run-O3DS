#!/usr/bin/env python3
"""Check bounded sampling and unchanged vehicle-update statements."""
from pathlib import Path
import re,subprocess,tempfile,sys
root=Path(__file__).resolve().parents[3]
if '--reference' in sys.argv:
 reference=Path(sys.argv[sys.argv.index('--reference')+1]).read_text()
 current=(root/'code/worldsim/redbrick/vehicle.cpp').read_text()
 stripped=re.sub(r'#ifdef SRR2_OLD3DS_VEHICLE_PROFILE\n.*?#endif\n','',current,flags=re.S)
 assert '\n'.join(line.rstrip() for line in stripped.splitlines())=='\n'.join(line.rstrip() for line in reference.splitlines())
 print('PASS: vehicle statements and order unchanged',flush=True)
harness=r'''
#include <worldsim/redbrick/old3dsvehicleprofile.h>
#include <cassert>
#include <cstdio>
Old3dsVehicleProfile::Counters Old3dsVehicleProfile::counters={};
Old3dsVehicleProfile::ExternalCounters Old3dsVehicleProfile::externalCounters={};
unsigned reads=0;u64 tick=0;
u64 svcGetSystemTick(){++reads;return tick;}
int main(){
 using namespace Old3dsVehicleProfile;
 static_assert(sizeof(Counters)<1024,"bounded vehicle profiler");
 unsigned sampledVehicles[2][32]={};
 for(unsigned mode=0;mode<2;++mode)for(unsigned call=0;call<1024;++call){
  const unsigned before=reads;Sample sample(mode==1);
  if(reads!=before)++sampledVehicles[mode][call%32];
  for(unsigned phase=0;phase<Remainder;++phase){tick+=7;sample.End(static_cast<Phase>(phase));}
  tick+=7;
 }
 for(unsigned mode=0;mode<2;++mode){assert(counters.calls[mode]==1024&&counters.samples[mode]==32);for(unsigned phase=0;phase<Count;++phase)assert(counters.ticks[mode][phase]==224);}
 assert(reads==2*32*(Count+1));
 for(unsigned mode=0;mode<2;++mode)for(unsigned vehicle=0;vehicle<32;++vehicle)assert(sampledVehicles[mode][vehicle]==1);
 reads=0;
 for(unsigned call=0;call<1024;++call){
  ExternalSample sample;tick+=11;sample.End(DynamicObjects);tick+=13;sample.End(AnimCollisions);
 }
 assert(externalCounters.calls==1024 && externalCounters.samples==32);
 assert(externalCounters.ticks[DynamicObjects]==352 && externalCounters.ticks[AnimCollisions]==416);
 assert(reads==32*3 && sizeof(ExternalCounters)<=32);
 CaptureEnabled()=false;const unsigned beforeCalls=counters.calls[0],beforeExternal=externalCounters.calls,beforeReads=reads;
 {Sample sample(false);sample.End(StateStore);ExternalSample external;external.End(DynamicObjects);}
 assert(counters.calls[0]==beforeCalls && externalCounters.calls==beforeExternal && reads==beforeReads);
 CaptureEnabled()=true;
 puts("PASS: rotating traffic/physics sampling at 1/32 across all 32 repeated vehicle slots, exact external/update phase ticks, bounded counters");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-vehicle-phases-')as td:
 p=Path(td);(p/'3ds.h').write_text('#pragma once\n#include <cstdint>\nusing u64=uint64_t;const unsigned SYSCLOCK_ARM11=268123480;u64 svcGetSystemTick();\n');(p/'test.cpp').write_text(harness)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(p),'-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
