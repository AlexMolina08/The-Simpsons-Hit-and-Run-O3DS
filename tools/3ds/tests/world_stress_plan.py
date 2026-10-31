#!/usr/bin/env python3
"""Exercise bounded, transactional mission plans and gameplay timing summaries."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='world-stress-plan-') as directory:
 p=Path(directory);(p/'test.cpp').write_text('''#include <main/worldstressplan.h>
#include <cassert>
#include <string>
int main(){
 WorldStress::PhaseWatchdog watchdog;
 assert(!watchdog.Expired(1,10,1000));assert(!watchdog.Expired(1,300010,1000));assert(watchdog.Expired(1,300011,1000));
 assert(!watchdog.Expired(2,500000,1000));assert(!watchdog.Expired(2,500001,1000));assert(watchdog.Expired(2,800001,1000));
 WorldStress::Plan plan;assert(plan.count==2 && plan.levels[0]==0 && plan.levels[1]==1 && plan.WatchdogSeconds()==300);
 assert(plan.Parse("4:7,1:2,7:7,2:4"));assert(plan.count==4 && plan.levels[0]==3 && plan.missions[0]==6 && plan.levels[2]==6 && plan.missions[1]==2 && plan.MissionNumber(1)==2);
 for(const char* bad:{"","1","1:","2:0","0:1","8:1","1:8","1:1,","1:1x","1:1,,2:2"," 1:1","-1:1","1:10"}){
  assert(!plan.Parse(bad));assert(plan.count==4 && plan.levels[0]==3 && plan.missions[0]==6);
 }
 std::string full="1:1";for(unsigned i=1;i<64;++i)full+=",7:7";
 assert(plan.Parse(full.c_str()) && plan.count==64);full+=",1:1";assert(!plan.Parse(full.c_str()) && plan.count==64);
 WorldStress::Plan first;assert(first.Parse("1:1,1:2,1:3,1:4,1:5,1:6,1:7"));for(unsigned i=0;i<7;++i)assert(first.missions[i]==i+1 && first.MissionNumber(i)==i+1);
 assert(first.Parse("1:0,1:1") && first.missions[0]==0 && first.MissionNumber(0)==0 && first.MissionNumber(1)==1);
 WorldStress::FrameStats stats;for(unsigned i=0;i<100;++i)stats.Sample(i<90?40000:60000,20000,1000000,3000000-i,i%2,10,2);
 assert(stats.samples==100 && stats.over50==10 && stats.maximumUs==60000 && stats.linearMinimum==2999901 && stats.streamingFrames==50 && stats.vehiclesPeak==10 && stats.stageFirst==2 && stats.stageLast==2 && stats.stageMaximum==2);
 assert(stats.PercentileUs(50)==42000 && stats.PercentileUs(95)==62000 && stats.PercentileUs(99)==62000);
 stats.Sample(500000,1000,1000000,2000000);assert(stats.maximumUs==500000 && stats.PercentileUs(100)==500000);
}
''')
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'code'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: 64-destination mission plans, malformed/trailing/overflow rejection, transactional state and bounded frame summaries.')
