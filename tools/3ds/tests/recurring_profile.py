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
fixture = r'''
#include <cassert>
#include <main/old3dsrecurring.h>
static u64 tick=100;
static u64 svcGetSystemTick(){return tick;}
namespace Old3dsRecurring {
bool active=true;
struct Node {u64 inclusive=0,exclusive=0;unsigned calls=0;};
struct {Node nodes[Count];unsigned scopes=0,errors=0;} current;
Scope* top=nullptr;unsigned backendDepth=0;u64 backendTicks=0;
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
