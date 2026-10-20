#!/usr/bin/env python3
"""Verify source/payload hashing never overlaps the read bucket; OFF has no clock calls."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/pddi/pica/texture.cpp').read_text();start=source.index('struct NativeTextureCost {');end=source.index('void picaSnapshotNativeTextures',start);fragment=source[start:end]
with tempfile.TemporaryDirectory(prefix='srr-native-cost-') as folder:
 p=Path(folder)
 (p/'test.cpp').write_text('''#include <cassert>
#include <cstdint>
#include <cstdio>
using u64=uint64_t;
static u64 now=0;static unsigned clockCalls=0;
static const u64 SYSCLOCK_ARM11=1000000;
u64 svcGetSystemTick(){++clockCalls;return now;}
static unsigned nativeTextureCost[3][5]={};
'''+fragment+'''
int main(){
 {NativeTextureCost c(true);c.outcome=0;now+=5;
  {NativeTextureCost::Phase open(c,NativeTextureCost::Open);now+=7;}
  {NativeTextureCost::Phase read(c,NativeTextureCost::ReadCheck);now+=11;
   {NativeTextureCost::Phase hash(c,NativeTextureCost::HashWork);now+=13;}
   now+=17;}
  now+=19;
 }
 assert(nativeTextureCost[0][0]==1&&nativeTextureCost[0][1]==72);
 assert(nativeTextureCost[0][2]==13&&nativeTextureCost[0][3]==7&&nativeTextureCost[0][4]==28);
 unsigned previous=clockCalls;
 {NativeTextureCost c(false);now+=10;
  {NativeTextureCost::Phase read(c,NativeTextureCost::ReadCheck);now+=20;
   {NativeTextureCost::Phase hash(c,NativeTextureCost::HashWork);now+=30;}}
 }
 assert(clockCalls==previous&&nativeTextureCost[2][0]==0);
 puts("PASS: disjoint hash/open/read phases, derived total, enclosing read pause/resume and zero clock calls while disabled.");
}
''')
 subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
