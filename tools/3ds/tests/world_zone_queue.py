#!/usr/bin/env python3
"""Prove the legacy zone overflow and exercise checked FIFO ownership and growth."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='world-zone-queue-') as directory:
    folder = Path(directory)
    (folder/'p3d').mkdir()
    (folder/'p3d/p3dtypes.hpp').write_text('#pragma once\n')
    (folder/'legacy.cpp').write_text('''#include <cstdlib>
#define rTuneAssert(...) ((void)0)
#include <render/Culling/SwapArray.h>
int main(){SwapArray<void*> queue(10);void* event=&queue;for(unsigned i=0;i<24;++i)queue.Add(event);}
''')
    subprocess.run(['clang++', '-O0', '-fsanitize=address', '-I'+str(folder), '-I'+str(root/'code'),
                    str(folder/'legacy.cpp'), '-o', str(folder/'legacy')], check=True)
    failed = subprocess.run([str(folder/'legacy')], capture_output=True)
    assert failed.returncode and b'heap-buffer-overflow' in failed.stderr
    (folder/'test.cpp').write_text(r'''
#include <cassert>
#include <cstdlib>
#include <vector>
static bool failAllocation=false;
static void* ControlledMalloc(size_t bytes){return failAllocation?nullptr:std::malloc(bytes);}
#define malloc ControlledMalloc
#include <render/RenderManager/worldzonequeue.h>
#undef malloc
struct Locator{
 static unsigned live;unsigned references=1,id;
 Locator(unsigned id):id(id){++live;}
 void AddRef(){++references;}
 void Release(){assert(references);if(!--references){--live;delete this;}}
};unsigned Locator::live=0;
int main(){
 {
  WorldZoneQueue<Locator> queue;assert(queue.Allocate(10));
  auto* active=new Locator(1);queue.SetActive(active);active->Release();
  for(unsigned i=0;i<10000;++i)assert(queue.Add(active));
  assert(!queue.mUseSize&&queue.duplicates==10000&&active->references==1);
  auto* next=new Locator(2);assert(queue.Add(next));next->Release();
  for(unsigned i=0;i<10000;++i)assert(queue.Add(next));
  // A/B/A must keep the second A; adjacent duplicate B demand coalesces.
  assert(queue.Add(active)&&queue.mUseSize==2);
  assert(queue.TakeNext()==next&&next->references==1);
  assert(queue.TakeNext()==active&&active->references==1);
  assert(queue.Add(active,false)&&queue.mUseSize==1);assert(queue.TakeNext()==active);
  queue.SetActive(nullptr);assert(!Locator::live);
  std::vector<Locator*> expected;
  for(unsigned i=0;i<4096;++i){auto* locator=new Locator(i);expected.push_back(locator);assert(queue.Add(locator));locator->Release();}
  assert(queue.peak==4096&&queue.mSize>=4096&&queue.growths>0);
  for(auto* value:expected){assert(queue.TakeNext()==value);assert(value->references==1);queue.SetActive(value);assert(value->references==1);}
  assert(!queue.TakeNext()&&Locator::live==1);
  queue.ClearUse();assert(Locator::live==1);queue.SetActive(nullptr);assert(!Locator::live);
 }
 {
  WorldZoneQueue<Locator,2> queue;
  auto* a=new Locator(1);auto* b=new Locator(2);auto* c=new Locator(3);
  assert(queue.Add(a)&&queue.Add(b));a->Release();b->Release();
  failAllocation=true;assert(!queue.Add(c));failAllocation=false;
  assert(queue.Failed()&&queue.failures==1&&queue.mUseSize==2&&queue.mSize==2&&c->references==1);
  c->Release();assert(queue.TakeNext()==a&&queue.TakeNext()==b);
  queue.SetActive(nullptr);assert(!Locator::live);
 }
 {
  WorldZoneQueue<Locator,2> queue;auto* active=new Locator(4);queue.SetActive(active);active->Release();
  for(unsigned i=0;i<100;++i){auto* value=new Locator(i);assert(queue.Add(value));value->Release();}
  queue.ClearUse();assert(!queue.mUseSize&&Locator::live==1);
 }
 assert(!Locator::live);
}
''')
    subprocess.run(['clang++', '-std=c++17', '-O1', '-fsanitize=address,undefined',
                    '-I'+str(root/'code'), str(folder/'test.cpp'), '-o', str(folder/'test')], check=True)
    subprocess.run([str(folder/'test')], check=True)
source = (root/'code/render/RenderManager/RenderManager.cpp').read_text()
assert source.count('mZELs.TakeNext()') == 3
assert 'mZELs.mUseSize = 0' not in source
assert 'mZELs.SetActive(pZEL)' in source
assert 'if(!mZELs.Add(pZEL,activeDemand))' in source and 'Game::GetInstance()->Stop()' in source
print('PASS: legacy 10-entry overflow reproduced; 20,000 duplicate events coalesce, A/B/A order and 4,096 distinct events survive growth, locator ownership and allocation-failure rollback pass ASan/UBSan.')
