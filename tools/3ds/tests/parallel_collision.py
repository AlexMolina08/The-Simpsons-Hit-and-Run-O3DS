#!/usr/bin/env python3
"""Stress production collision dispatch publication, stealing, cancellation and joins."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[3]
s=(root/'libs/sim/simcollision/parallelcollision.cpp').read_text()
def method(signature):
 a=s.index(signature);b=s.index('{',a);end=b+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]
source=r'''
#include <cassert>
#include <cstdio>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdint>
using u64=uint64_t;
struct LightEvent {std::mutex mutex;std::condition_variable condition;bool ready=false;};
void LightEvent_Wait(LightEvent* e){std::unique_lock<std::mutex> lock(e->mutex);e->condition.wait(lock,[&]{return e->ready;});e->ready=false;}
void LightEvent_Signal(LightEvent* e){std::lock_guard<std::mutex> lock(e->mutex);e->ready=true;e->condition.notify_one();}
u64 svcGetSystemTick(){return std::chrono::steady_clock::now().time_since_epoch().count();}
unsigned svcGetProcessorID(){return 1;}
struct {unsigned cancelled=0,mainAreas=0,workerAreas=0;u64 joinTicks=0;unsigned mainGroups=0,workerGroups=0;} statistics;
struct Pairs {unsigned generation=0,value=0;};
struct Contacts {unsigned generation=0,value=0,calls=0;};
struct CollisionDetector {
 unsigned tail=0;
 bool DetectPreparedArea(Pairs* p,Contacts* c,float) {
  unsigned generation=p->generation,value=p->value;
  if(value&1)std::this_thread::yield();
  if((value&7)==3)std::this_thread::sleep_for(std::chrono::microseconds(5));
  for(unsigned i=0;i<25;++i)value=value*1664525u+1013904223u;
  assert(generation==p->generation);c->generation=generation;c->value=value;++c->calls;tail=value;
  return (value&15)!=0;
 }
 unsigned PreparedTailCount(){return tail;}
};
struct ParallelCollisionState {
 struct Job {Pairs* pairs=nullptr;Contacts* contacts=nullptr;bool overflow=false;unsigned tail=0;} jobs[64];
 struct Volume {unsigned calls=0;void UpdateAll(){++calls;std::this_thread::yield();}};
 struct Root {Volume* volume;unsigned next;} roots[1024];
 struct Group {unsigned first;} groups[64];
 static constexpr unsigned MaxVolumes=1024;
 unsigned rootStage=0,mainControls=0;static unsigned FloatControls(){return 0;}
 LightEvent wake,finished;CollisionDetector mainDetector,workerDetector;
 unsigned next=0,count=0,state=0,quit=0,workerAreas=0,mainAreas=0,workerCore=~0u;float dt=.035f;
 void Run(CollisionDetector&,bool);void Dispatch();static void Worker(void*);
};
'''
source+='\n'.join(method(sig) for sig in ['void ParallelCollisionState::Worker(', 'void ParallelCollisionState::Run(', 'void ParallelCollisionState::Dispatch('])
source+=r'''
int main(){
 ParallelCollisionState s;Pairs input[64];Contacts output[64];
 std::thread worker([&]{ParallelCollisionState::Worker(&s);});
 unsigned seed=34151,total=0;
 for(unsigned batch=1;batch<=5000;++batch) {
  seed=seed*1664525u+1013904223u;s.count=(batch%5==0)?0:seed%65;total+=s.count;
  for(unsigned i=0;i<s.count;++i){input[i]={batch,i+seed};output[i]={};s.jobs[i].pairs=&input[i];s.jobs[i].contacts=&output[i];}
  s.Dispatch();assert(s.mainAreas+s.workerAreas==s.count);
  for(unsigned i=0;i<s.count;++i) {
   unsigned expected=input[i].value;for(unsigned j=0;j<25;++j)expected=expected*1664525u+1013904223u;
   assert(output[i].calls==1 && output[i].generation==batch && output[i].value==expected);
   assert(s.jobs[i].tail==expected && s.jobs[i].overflow==((expected&15)==0));
  }
 }
 ParallelCollisionState::Volume volumes[128];s.rootStage=1;
 unsigned rootTotal=0;
 for(unsigned batch=0;batch<2000;++batch) {
  s.count=batch%64;rootTotal+=s.count;
  for(unsigned i=0;i<s.count;++i){volumes[2*i].calls=volumes[2*i+1].calls=0;s.groups[i].first=2*i;s.roots[2*i]={&volumes[2*i],2*i+1};s.roots[2*i+1]={&volumes[2*i+1],s.MaxVolumes};}
  s.Dispatch();
  for(unsigned i=0;i<s.count*2;++i)assert(volumes[i].calls==1);
 }
 assert(statistics.mainGroups+statistics.workerGroups==rootTotal);
 __atomic_store_n(&s.quit,1u,__ATOMIC_RELEASE);LightEvent_Signal(&s.wake);worker.join();
 assert(statistics.mainAreas+statistics.workerAreas==total);
 assert(statistics.workerAreas>100 && statistics.cancelled>100);
 printf("PASS: 5000 production dispatch batches; %u exact-once jobs, %u worker, %u cancelled late wakes; owned outputs and immediate buffer reuse. Not collision mathematics or hardware timing.\n",total,statistics.workerAreas,statistics.cancelled);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-parallel-collision-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-pthread','-fsanitize='+('thread' if '--tsan' in sys.argv else 'address,undefined'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
