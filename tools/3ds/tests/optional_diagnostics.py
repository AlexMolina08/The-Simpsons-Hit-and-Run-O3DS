#!/usr/bin/env python3
"""Verify diagnostic removal preserves classification and disjoint pool clocks."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]

DIAG = r'''
#include <cassert>
#include "main/old3dsdiag.h"
extern "C" {
unsigned g_old3dsDrawCalls=0,g_old3dsDrawVerts=0;
unsigned long long g_old3dsPhase2Ticks[4]={};unsigned g_old3dsPhase2Draws[4]={};
Old3dsEntityStats g_old3dsEntities[O3_CATEGORY_COUNT]={};
Old3dsOperationStats g_old3dsOperations[O3_OPERATION_COUNT]={};
unsigned g_old3dsBackend[O3_COUNTER_COUNT]={};int g_old3dsActiveEntity=-1;
Old3dsAnimStats g_old3dsAnimStats[8]={};unsigned g_old3dsAnimQueries[4]={};
unsigned g_old3dsTriQueries=0,g_old3dsTriCandidates=0,g_old3dsTriWorst=0,g_old3dsTriEmptyLeaf=0;
unsigned g_old3dsTriHits=0,g_old3dsTriMisses=0,g_old3dsTriSamples=0;double g_old3dsTriMs=0;
}
int main(){
 {Old3dsEntityScope outer(O3_VEHICLE);assert(g_old3dsActiveEntity==O3_VEHICLE);
  {Old3dsEntityScope inner(O3_STATIC);assert(g_old3dsActiveEntity==O3_STATIC);++g_old3dsDrawCalls;}
  assert(g_old3dsActiveEntity==O3_VEHICLE);
 }
 assert(g_old3dsActiveEntity==-1);
 assert(g_old3dsEntities[O3_VEHICLE].calls==1 && g_old3dsEntities[O3_STATIC].calls==1);
 assert(g_old3dsEntities[O3_VEHICLE].draws==1 && g_old3dsEntities[O3_STATIC].draws==1);
 {Old3dsOperationScope op(O3_SKIN,7);op.Finish();}
 {Old3dsAnimScope anim(91,"test",false,false);}
 {Old3dsRenderScope render(0);++g_old3dsDrawCalls;}
 bool found=true,reached=true;{Old3dsTriScope tri(found,reached);}
 assert(g_old3dsTriQueries==1 && g_old3dsTriHits==1);
#ifdef SRR2_OLD3DS_OPTIONAL_DIAG
 assert(reads>0 && g_old3dsOperations[O3_SKIN].units==7);
#else
 assert(reads==0 && g_old3dsOperations[O3_SKIN].units==0);
#endif
}
'''

POOLS = r'''
#include <cassert>
#include "main/old3dspools.h"
'''
native = (ROOT/'code/main/nativebench.cpp').read_text()
start = native.index('namespace Old3dsPools {')
end = native.index('\n#endif', start)
POOLS += native[start:end] + r'''
int main(){
 using namespace Old3dsPools;
 Configure(Recipe);Begin(17);
 const u64 first=clockValue;
 {Scope upstream(Recipe,Upstream);clockValue+=100;
  {Scope backend(Recipe,Backend);clockValue+=70;}
  clockValue+=30;
  {Scope fallback(Recipe,Fallback);clockValue+=20;}
  const unsigned before=reads;
  currentTls=reinterpret_cast<void*>(2);
  {Scope foreign(Recipe,Upstream);}
  assert(reads==before);
  currentTls=reinterpret_cast<void*>(1);
 }
 End();
 u64 total=0;for(unsigned i=0;i<Count;++i)total+=ticks[i];
 assert(total==clockValue-first);
 assert(ticks[Upstream]<total && ticks[Backend]>=70 && ticks[Fallback]>=20);
 const unsigned before=reads;
 Begin(18);{Scope skipped(Recipe,Upstream);}End();assert(reads==before);
}
'''

with tempfile.TemporaryDirectory(prefix='srr-diag-gates-') as temporary:
    folder = Path(temporary)
    (folder/'3ds.h').write_text('''#pragma once
#include <cstdint>
using u64=uint64_t;
const unsigned SYSCLOCK_ARM11=268123480;
inline unsigned reads=0;
inline u64 clockValue=0;
inline void* currentTls=reinterpret_cast<void*>(1);
inline u64 svcGetSystemTick(){++reads;return ++clockValue;}
inline void* getThreadLocalStorage(){return currentTls;}
''')
    for mode, source, flags in (
        ('optional-on', DIAG, ['-DSRR2_OLD_3DS', '-DSRR2_OLD3DS_OPTIONAL_DIAG']),
        ('optional-off', DIAG, ['-DSRR2_OLD_3DS']),
        ('pools', POOLS, ['-DSRR2_OLD3DS_COARSE_POOLS']),
    ):
        cpp, binary = folder/(mode+'.cpp'), folder/mode
        cpp.write_text(source)
        subprocess.run(['clang++', '-std=c++17', '-O2', '-fsanitize=address,undefined',
                        *flags, '-I', str(folder), '-I', str(ROOT/'code'), str(cpp), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print('PASS: optional timers make zero tick reads when OFF; classification, workload and restoration survive; coarse buckets stay disjoint and exclude foreign threads.')
