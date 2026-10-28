#!/usr/bin/env python3
"""Check SDK fog state parity while avoiding only unchanged setter requests."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'libs/pure3d/pddi/pica/nativecontext.c').read_text()
a=source.index('static bool fogReuse=');b=source.index('#endif',a)
fragment=source[a:b]
fixture=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
using u32=uint32_t;using GPU_FOGMODE=unsigned;using GPU_GASMODE=unsigned;
#define BIT(i) (1u<<(i))
const unsigned C3DiF_Active=1,C3DiF_TexEnvBuf=2;
struct C3D_Context {u32 flags,texEnvBuf,fogClr;};
C3D_Context context;unsigned writes;
C3D_Context* C3Di_GetContext(){return &context;}
void C3D_FogGasMode(unsigned fog,unsigned gas,bool flip){if(!(context.flags&C3DiF_Active))return;++writes;context.flags|=C3DiF_TexEnvBuf;context.texEnvBuf=(context.texEnvBuf&~0x100FFu)|(fog&7u)|((gas&1u)<<3)|(flip?BIT(16):0);}
void C3D_FogColor(u32 color){if(!(context.flags&C3DiF_Active))return;++writes;context.flags|=C3DiF_TexEnvBuf;context.fogClr=color;}
'''
test=r'''
unsigned seed=51179;unsigned randomValue(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(){
 unsigned comparisons=0,referenceWrites=0,candidateWrites=0;
 for(unsigned trial=0;trial<20000;++trial){
  C3D_Context reference={C3DiF_Active,randomValue(),randomValue()},candidate=reference;
  u32 referenceMode=reference.texEnvBuf,referenceColor=reference.fogClr;
  u32 candidateMode=referenceMode,candidateColor=referenceColor;
  for(unsigned operation=0;operation<32;++operation){
   if(operation%9==0){reference.flags|=C3DiF_TexEnvBuf;candidate.flags|=C3DiF_TexEnvBuf;}
   if(operation%11==0){reference.flags^=C3DiF_Active;candidate.flags^=C3DiF_Active;}
   const bool mode=operation%3!=0;
   const unsigned fog=operation%2?reference.texEnvBuf&7:randomValue();
   const unsigned gas=operation%2?(reference.texEnvBuf>>3)&1:randomValue();
   const bool flip=operation%2?bool(reference.texEnvBuf&BIT(16)):bool(randomValue()&1);
   const unsigned color=operation%2?reference.fogClr:randomValue();
   context=reference;writes=0;
   if(mode)C3D_FogGasMode(fog,gas,flip);else C3D_FogColor(color);
   reference=context;referenceWrites+=writes;
   context=candidate;writes=0;picaNativeEnableFogReuse(true);
   if(mode)picaNativeFogMode(fog,gas,flip);else picaNativeFogColor(color);
   candidate=context;candidateWrites+=writes;
   assert(reference.texEnvBuf==candidate.texEnvBuf && reference.fogClr==candidate.fogClr);
   assert((reference.flags&~C3DiF_TexEnvBuf)==(candidate.flags&~C3DiF_TexEnvBuf));
   if((reference.flags&(C3DiF_Active|C3DiF_TexEnvBuf))==(C3DiF_Active|C3DiF_TexEnvBuf)){
    referenceMode=reference.texEnvBuf;referenceColor=reference.fogClr;reference.flags&=~C3DiF_TexEnvBuf;
   }
   if((candidate.flags&(C3DiF_Active|C3DiF_TexEnvBuf))==(C3DiF_Active|C3DiF_TexEnvBuf)){
    candidateMode=candidate.texEnvBuf;candidateColor=candidate.fogClr;candidate.flags&=~C3DiF_TexEnvBuf;
   }
   assert(referenceMode==candidateMode && referenceColor==candidateColor);++comparisons;
  }
 }
 assert(comparisons==640000 && candidateWrites<referenceWrites);
 picaNativeEnableFogReuse(false);context={C3DiF_Active,0,0};writes=0;picaNativeFogMode(0,0,false);assert(writes==1 && (context.flags&C3DiF_TexEnvBuf));
 puts("PASS: 640,000 fog state/draw comparisons; mode masks, gas, z-flip, colors, inactive contexts, restore dirtiness and disabled control");
 printf("SDK setter calls: %u reference, %u reused; accepted native/SDK fog barriers unchanged\n",referenceWrites,candidateWrites);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-fog-reuse-') as directory:
 p=Path(directory);(p/'test.cpp').write_text(fixture+fragment+test)
 subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
