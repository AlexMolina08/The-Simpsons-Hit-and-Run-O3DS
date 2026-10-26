#!/usr/bin/env python3
"""Check production scope accounting with an exact synthetic clock."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/main/old3dsrecurring.cpp').read_text()
methods = '\n'.join(re.search(pattern, source).group() for pattern in (
    r'static bool BackendStage\([^\n]+',
    r'void Scope::Enter\([^\n]+',
    r'void Scope::Leave\([^\n]+',
))
start=source.index('void Begin(unsigned frame,');end=source.index('\nvoid End()',start)
methods+='\n'+source[start:end]
start=source.index('static unsigned FindAssetSlot(');end=source.index('\nEntity::Entity(',start)
methods+='\n'+source[start:end]
fixture = r'''
#include <cassert>
#include <main/old3dsrecurring.h>
static u64 tick=100;
static u64 svcGetSystemTick(){return tick;}
namespace Old3dsRecurring {
bool active=true,requested=true,focusDropWindow=false;u64 calibration=1;
unsigned windowStartMs=15000,windowEndMs=18000;
struct Node {u64 inclusive=0,exclusive=0;unsigned calls=0;};
struct {Node nodes[Count];unsigned scopes=0,errors=0,frame=0,ms=0;} current;
Scope* top=nullptr;unsigned backendDepth=0;u64 backendTicks=0;
constexpr unsigned Assets=256,O3_CATEGORY_COUNT=11,O3_ANIM=3,O3_PARTICLE=10;
struct Asset {unsigned long long uid=0;unsigned category=0,pass=0,calls=0;};
Asset assets[Assets+2*O3_CATEGORY_COUNT];unsigned overflow=0;
METHODS
}
int main(){
 using namespace Old3dsRecurring;
 {
  Scope root(Update);
  tick=110;{Scope material(Material);tick=130;}
  tick=140;{Scope submit(Submit);tick=145;{Scope uniforms(Uniforms);tick=155;}tick=170;}
  tick=180;{
   Scope backend(Backend);
   tick=185;{Scope material(Material);tick=190;}
   tick=195;{Scope submit(Submit);tick=200;{Scope uniforms(Uniforms);tick=210;}tick=220;}
   tick=230;
  }
  tick=250;
 }
 assert(backendTicks==100 && backendDepth==0 && top==nullptr && current.errors==0);
 assert(current.nodes[Update].inclusive==150 && current.nodes[Update].exclusive==50);
 u64 exclusive=0;for(const auto& node:current.nodes)exclusive+=node.exclusive;
 assert(exclusive==150);
 assert(current.nodes[Material].exclusive==25 && current.nodes[Uniforms].exclusive==20);
 assert(current.nodes[Submit].exclusive==35 && current.nodes[Backend].exclusive==20);
 focusDropWindow=true;Begin(17,14999,true);assert(!active);
 Begin(21,15000,true);assert(active && current.frame==21 && current.ms==15000);
 Begin(22,16000,true);assert(!active);
 Begin(25,17999,true);assert(active);
 Begin(29,18000,true);assert(!active);
 windowStartMs=43000;windowEndMs=47000;
 Begin(17,42999,true);assert(!active);Begin(21,43000,true);assert(active);
 Begin(22,44000,true);assert(!active);Begin(25,46999,true);assert(active);
 Begin(29,47000,true);assert(!active);Begin(33,44000,false);assert(!active);
 focusDropWindow=false;Begin(81,20000,true);assert(active);
 requested=false;Begin(81,20000,true);assert(!active);
 // Fill the nonanimated partition with deliberately colliding keys. Animated
 // IDs must still retain identity, category and pass in their reserved half.
 auto insert=[](unsigned long long uid,unsigned category,unsigned pass){
  unsigned slot=FindAssetSlot(uid,category,pass);
  auto& a=assets[slot];if(!a.calls){a.uid=uid;a.category=category;a.pass=pass;}
  ++a.calls;return slot;
 };
 focusDropWindow=true;
 for(unsigned i=0;i<128;++i)assert(insert(256ull*i,5,0)<128);
 assert(insert(256ull*128,5,0)==Assets+5 && overflow==1);
 for(unsigned i=0;i<64;++i){
  const unsigned slot=insert(256ull*i,O3_ANIM,1);assert(slot>=128 && slot<Assets);
  assert(insert(256ull*i,O3_ANIM,1)==slot);
  assert(insert(256ull*i,O3_PARTICLE,1)!=slot);
 }
 assert(overflow==1);
 assert(insert(99999,O3_ANIM,0)==Assets+O3_ANIM && overflow==2);
 for(auto& a:assets)a={};overflow=0;focusDropWindow=false;
 for(unsigned i=0;i<8;++i)assert(insert(256ull*i,5,0)<Assets);
 assert(insert(256ull*8,5,0)==Assets+5 && overflow==1);
}
'''.replace('METHODS', methods)
with tempfile.TemporaryDirectory(prefix='srr-recurring-') as directory:
    temp = Path(directory)
    (temp / '3ds.h').write_text('#include <cstdint>\nusing u64=std::uint64_t;\n')
    (temp / 'test.cpp').write_text(fixture)
    subprocess.run(['clang++', '-std=c++17', '-DSRR2_OLD3DS_NATIVE_BENCH',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I'+str(temp), '-I'+str(root/'code'), str(temp/'test.cpp'),
                    '-o', str(temp/'test')], check=True)
    subprocess.run([str(temp/'test')], check=True)
print('PASS: exact root reconciliation and topmost backend accounting, including immediate material and nested submit/uniform scopes.')
