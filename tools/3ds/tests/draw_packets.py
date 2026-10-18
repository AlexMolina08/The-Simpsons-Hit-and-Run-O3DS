#!/usr/bin/env python3
"""Compare packet submission with the audited upstream Citro3D functions."""
from pathlib import Path
import subprocess,tempfile,re
root=Path(__file__).resolve().parents[3]
upstream=root/'tools/3ds/tests/reference/citro3d_171'
if not upstream.is_dir():raise SystemExit('Audited Citro3D 1.7.1 sources are required for reference comparisons')
def clean(text):return '\n'.join(x for x in text.splitlines() if not x.startswith('#include'))
source=r'''
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <cassert>
#include <cstdio>
#include <algorithm>
using u32=uint32_t;
using GPU_Primitive_t=unsigned;
constexpr unsigned GPU_TRIANGLES=0,GPU_GEOMETRY_PRIM=0x300;
#define GPUCMD_HEADER(i,m,r) (((i)<<31)|(((m)&15)<<16)|((r)&1023))
#define CMD_WORDS(reg,mask,value) (value),GPUCMD_HEADER(0,(mask),(reg))
#define GPUCMD_AddMaskedWrite(r,m,v) GPUCMD_AddSingleParam(GPUCMD_HEADER(0,(m),(r)),(v))
#define GPUCMD_AddWrite(r,v) GPUCMD_AddMaskedWrite((r),15,(v))
'''
regs=(Path('/opt/devkitpro/libctru/include/3ds/gpu/registers.h')).read_text()
source+='\n'.join(x for x in regs.splitlines() if re.match(r'#define GPUREG_\w+\s+0x[0-9A-Fa-f]+',x))+'\n'
fixture=r'''
struct C3D_BufInfo {u32 base_paddr;};
struct C3D_Context {char padding[32];u32 flags;char padding2[28];C3D_BufInfo bufInfo;};
constexpr unsigned C3DiF_DrawUsed=2;
C3D_Context context;std::vector<u32> words;unsigned updates=0,appends=0;
C3D_Context* C3Di_GetContext(){return &context;}
u32 osConvertVirtToPhys(const void*p){return uintptr_t(p);}
void C3Di_UpdateContext(){++updates;words.push_back(0xabcdef00);words.push_back(context.flags);context.flags^=0x400;context.bufInfo.base_paddr+=4;}
void GPUCMD_AddSingleParam(u32 header,u32 value){++appends;words.push_back(value);words.push_back(header);}
void GPUCMD_AddRawCommands(const u32*p,u32 n){++appends;words.insert(words.end(),p,p+n);}
'''
source+='namespace Reference {\n'+fixture+clean((upstream/'drawElements.c').read_text())+'\n'+clean((upstream/'drawArrays.c').read_text())+'\n}\n'
source+='namespace Candidate {\n'+fixture+clean((root/'libs/pure3d/pddi/pica/drawpacket.c').read_text()).replace('_Static_assert','static_assert')+'\n}\n'
source+=r'''
int main(){unsigned state=845,checks=0;unsigned long referenceAppends=0,candidateAppends=0;
for(unsigned trial=0;trial<40000;++trial){state=state*1664525u+1013904223u;unsigned primitive=(trial%4)*0x100;int count=trial%701;int first=trial%101;unsigned base=0x14000000;unsigned address=trial%13==0?base-2:base+(state&0xfffff);int type=trial%2;
for(unsigned array=0;array<2;++array){Reference::context={};Candidate::context={};Reference::context.flags=Candidate::context.flags=state;Reference::context.bufInfo.base_paddr=Candidate::context.bufInfo.base_paddr=base;Reference::words.clear();Candidate::words.clear();Reference::updates=Candidate::updates=Reference::appends=Candidate::appends=0;
if(array){Reference::C3D_DrawArrays(primitive,first,count);Candidate::picaDrawPacketArrays(primitive,first,count);}else{Reference::C3D_DrawElements(primitive,count,type,(void*)uintptr_t(address));Candidate::picaDrawPacketElements(primitive,count,type,(void*)uintptr_t(address));}
assert(Reference::words==Candidate::words);assert(Reference::updates==Candidate::updates);assert(Reference::context.flags==Candidate::context.flags);assert(Reference::context.bufInfo.base_paddr==Candidate::context.bufInfo.base_paddr);assert(Candidate::appends<=1);referenceAppends+=Reference::appends;candidateAppends+=Candidate::appends;++checks;}}
printf("PASS: %u upstream command-word/context comparisons; triangles, strips, fans, geometry primitives, arrays, counts, offsets, index types and invalid addresses. Appends %lu -> %lu; not hardware timing.\n",checks,referenceAppends,candidateAppends);}
'''
with tempfile.TemporaryDirectory(prefix='srr-draw-packets-') as d:
 p=Path(d);(p/'test.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
