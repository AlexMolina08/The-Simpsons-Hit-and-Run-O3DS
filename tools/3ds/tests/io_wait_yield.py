#!/usr/bin/env python3
"""Check production pending-read waits against controlled completion schedules."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/p3d/fileftt.cpp').read_text()
a=source.index('void tFileFTT::WaitForCompletion( void )');b=source.index('{',a);e=b+1;depth=1
while depth:
 depth+=(source[e]=='{')-(source[e]=='}');e+=1
body=source[a:e]
a=source.index('struct Old3dsIoWaitCounters');b=source.index('static Old3dsIoWaitCounters',a)
counters=source[a:b]
prelude=r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>
using radTime64=unsigned long long;
static radTime64 tick=0,gLastTime=0;
static unsigned gYieldTime=8000,g_p3dYieldTimeUs=8000;
static int g_radLoadMainWaiting=0;
static radTime64 radTimeGetMicroseconds64(){return ++tick;}
static radTime64 svcGetSystemTick(){return tick;}
#define rReleasePrintf(...) ((void)0)
'''+counters+r'''
static Old3dsIoWaitCounters old3dsIoWait={};
struct File {
 unsigned polls=0,handoffs=0,checksUntilReady=0,handoffsUntilReady=0;
 bool complete=false;
 bool CheckForCompletion(){
  ++tick;++polls;
  complete=polls>=checksUntilReady && handoffs>=handoffsUntilReady;
  return complete;
 }
};
static File* activeFile;
static std::vector<unsigned> callbacks;
struct Manager {
 void SwitchTask(){
  callbacks.push_back(++activeFile->handoffs);
  // A suspended loader may spend arbitrary time off CPU before resumption.
  tick+=50000;
 }
};
namespace p3d {static Manager manager;static Manager* loadManager=&manager;}
struct Reference {File* m_pIRadFile;void WaitForCompletion();};
struct Optimized {File* m_pIRadFile;void WaitForCompletion();};
'''
fixtures='#undef SRR2_OLD3DS_IO_WAIT_YIELD\n#define RAD_3DS\n'+body.replace('tFileFTT::','Reference::')+'\n#define SRR2_OLD3DS_IO_WAIT_YIELD\n'+body.replace('tFileFTT::','Optimized::')
main=r'''
int main(){
 unsigned cases=0;unsigned long long referenceChecks=0,optimizedChecks=0;
 for(unsigned budget:{8000u,52000u})for(unsigned handoffs=0;handoffs<8;++handoffs)for(unsigned polls=1;polls<200;polls+=7)for(unsigned force=0;force<2;++force){
  File before,after;before.checksUntilReady=after.checksUntilReady=polls;
  before.handoffsUntilReady=after.handoffsUntilReady=handoffs;
  g_radLoadMainWaiting=force;gYieldTime=force ? 0 : budget;g_p3dYieldTimeUs=budget;
  tick=gLastTime=0;activeFile=&before;callbacks.clear();Reference a{&before};a.WaitForCompletion();
  assert(before.complete);const unsigned baselineHandoffs=before.handoffs;referenceChecks+=before.polls;
  tick=gLastTime=0;activeFile=&after;callbacks.clear();old3dsIoWait={};Optimized b{&after};b.WaitForCompletion();
  assert(after.complete);optimizedChecks+=after.polls;
  assert(old3dsIoWait.waits==1 && old3dsIoWait.checks==after.polls);
  assert(old3dsIoWait.pendingWaits==(after.polls>1));
  assert(old3dsIoWait.handoffs==after.handoffs);
  assert(old3dsIoWait.maxPolls<=64);
  if(budget==52000&&!force){assert(before.polls==after.polls&&baselineHandoffs==after.handoffs);assert(old3dsIoWait.earlyGameplayHandoffs==0);}
  if(force)assert(old3dsIoWait.forcedHandoffs==after.handoffs && old3dsIoWait.maxPolls<=1);
  for(unsigned i=0;i<callbacks.size();++i)assert(callbacks[i]==i+1);
  // Caller may consume/swap buffers only after the actual read completes.
  assert(activeFile->CheckForCompletion());++cases;
 }
 printf("PASS: %u production wait schedules; completion before buffer use, ordered service callbacks, immediate forced reclaim and <=64 pending gameplay checks, exact loading-screen poll/handoff schedules; reference checks=%llu batched checks=%llu (simulated I/O only)\n",cases,referenceChecks,optimizedChecks);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-io-yield-') as td:
 p=Path(td);(p/'test.cpp').write_text(prelude+fixtures+main)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,timeout=30)
