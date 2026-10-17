#!/usr/bin/env python3
"""Exercise production caller-owned query arrays, ownership and allocation fallback."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
code = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
static unsigned allocations, releases;
void* operator new[](std::size_t n) { ++allocations; if(void* p=std::malloc(n))return p;throw std::bad_alloc(); }
void operator delete[](void* p) noexcept { ++releases;std::free(p); }
#define TOOLS
#define rAssert(x) assert(x)
#include "code/render/Culling/ReserveArray.h"
int main() {
 for(unsigned round=0;round<10000;++round) {
  int values[200], *storage[200];
  unsigned before=allocations;
  {ReserveArray<int*> a(storage,200);
   a.Allocate(200);
   for(int i=0;i<200;++i){values[i]=i;a.Add(storage[i]=&values[i]);}
   for(int i=0;i<200;++i)assert(a[i]==&values[i]&&*a[i]==i);
   a.ClearUse();a.Allocate(100);assert(a.mpData==storage&&a.mUseSize==0&&allocations==before);
  }
  assert(allocations==before&&releases==before);
  {ReserveArray<int*> a(storage,200);a.Allocate(201);assert(a.mpData!=storage&&a.mSize==201);
   for(int i=0;i<201;++i){int* p=&values[i%200];a.Add(p);}
   a.Clear();assert(!a.IsSetUp());a.Reserve(9);a.Allocate();assert(a.mSize==9);
  }
  assert(allocations==releases);
  {ReserveArray<int*> a(7);assert(a.mSize==7);a.Allocate(3);assert(a.mSize==3);}
  {ReserveArray<int*> a;a.Reserve(13);a.Allocate();assert(a.mSize==13&&a.mUseSize==0);}
  {ReserveArray<int*> a(storage,200);a.Clear();a.Allocate(5);assert(a.mSize==5);}
  assert(allocations==releases);
 }
 std::puts("PASS: 10,000 production array lifetimes; zero scratch allocations, ordered results, heap spill, clear/reserve and legacy ownership");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-query-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-DSRR2_OLD3DS_QUERY_SCRATCH','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
