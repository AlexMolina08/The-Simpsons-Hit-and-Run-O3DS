#!/usr/bin/env python3
"""Exercise production native allocation/retirement without GPU-visible writes."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[3]/'libs/pure3d/pddi/pica/context.cpp').read_text()
a=s.index('struct alignas(16) picaBufferAllocation')
b=s.index('{',s.index('static void picaBufferReclaim()\n{'));depth=1;i=b+1
while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
s=s[a:i]
harness=r'''
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <cstdint>
bool g_picaInFrame=true;
unsigned vramReclaims=0;extern "C" void picaReclaimVramTextures(){++vramReclaims;}
bool fail=false;unsigned allocations=0,frees=0,fences=0;
extern "C" void C3Di_RenderQueueWaitDone(){++fences;fail=false;}
void* linearAlloc(unsigned n){if(fail)return nullptr;++allocations;return malloc(n);}
void linearFree(void*p){++frees;free(p);}
'''
tests=r'''
int main(){
 assert(picaBufferAllocate(~0u)==nullptr);fail=true;assert(!picaBufferAllocate(100));fail=false;
 std::vector<unsigned char*> pending;
 for(unsigned i=0;i<10000;++i){auto*p=(unsigned char*)picaBufferAllocate(37);assert(p&&uintptr_t(p)%16==0);memset(p,0x6b,37);pending.push_back(p);picaBufferRetire(p);}
 assert(frees==0);for(auto*p:pending)for(unsigned i=0;i<37;++i)assert(p[i]==0x6b);
 picaBufferReclaim();assert(frees==allocations&&retiredBuffers==nullptr);
 picaBufferRetire(nullptr);picaBufferReclaim();assert(frees==allocations);
 auto*p=picaBufferAllocate(37);picaBufferRetire(p);fail=true;
 assert(!picaBufferAllocate(37)&&fences==0);g_picaInFrame=false;
 auto*q=picaBufferAllocate(37);assert(q&&fences==1&&retiredBuffers==nullptr);
 picaBufferRetire(q);picaBufferReclaim();assert(frees==allocations);
 puts("PASS: 10,000 native retirements; GPU ranges unchanged until fence, aligned storage, allocation/size failure and complete reclamation");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-native-retire-') as td:
 p=Path(td);(p/'test.cpp').write_text(harness+s+tests)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DSRR2_OLD3DS_VRAM_TEXTURES',str(p/'test.cpp'),'-o',str(p/'test-vram')],check=True)
 subprocess.run([str(p/'test-vram')],check=True)
