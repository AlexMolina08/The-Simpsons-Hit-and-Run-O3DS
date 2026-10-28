// Check the production callback gate across adversarial completion schedules.
#include <atomic>
#include <cassert>
#include <thread>
struct gxCmdQueue_s {unsigned short numEntries=0,lastEntry=0;void (*callback)(gxCmdQueue_s*)=nullptr;};
static unsigned runs=0;
void gxCmdQueueRun(gxCmdQueue_s*){++runs;}
#include "../../../libs/pure3d/pddi/pica/earlyqueue.hpp"
static std::atomic<unsigned> completions{0};
static bool frameEndPublished=false;
void original(gxCmdQueue_s*){assert(frameEndPublished);++completions;}
int main(){
 using namespace picaEarlyQueue;
 gxCmdQueue_s queue;queue.callback=original;
 for(unsigned iteration=0;iteration<20000;++iteration){
  const unsigned before=completions.load();frameEndPublished=false;
  queue.numEntries=3;queue.lastEntry=0;Start(&queue);assert(queue.callback==Complete);
  // Intermediate queue drains never trigger the SDK's swap callback.
  queue.lastEntry=3;queue.callback(&queue);assert(completions==before);
  queue.numEntries=7;queue.lastEntry=3;queue.callback(&queue);assert(completions==before);
  frameEndPublished=true;
  if(iteration%3==0){queue.lastEntry=7;queue.callback(&queue);Arm();}
  else if(iteration%3==1){
   Arm();queue.callback(&queue);assert(completions==before);
   queue.lastEntry=7;queue.callback(&queue);
  }
  else {
   std::thread irq([&]{__atomic_store_n(&queue.lastEntry,7,__ATOMIC_RELEASE);queue.callback(&queue);});
   Arm();irq.join();
  }
  queue.callback(&queue);assert(completions==before+1);Restore();
  assert(queue.callback==original&&Current().stage==0&&!Current().queue);
 }
 assert(runs==20000);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
 assert(Current().completed==20000);
#endif
}
