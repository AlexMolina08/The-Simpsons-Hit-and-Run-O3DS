#!/usr/bin/env python3
"""Compare pruned and fixed-predictor CTR output with the generic encoder."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
name='libs/radsound/src/hal/ctr/ndsphal.cpp'
current=(root/name).read_text()
def encoder(s):return s[s.index('static u16 s_AdpcmCoefs'):s.index('class radSoundHalBufferCtr')]
code=r'''
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <random>
#include <cstdio>
#include <cmath>
using u16=uint16_t;using s16=int16_t;using u8=uint8_t;
#define NB_SCOPE(s) ((void)0)
namespace Old3dsPerformance { bool ReducedAudioSearch(){return false;} }
namespace Reference {
'''+encoder(current)+'\n}\nnamespace Optimized {\n'+encoder(current)+r'''
}
int main(){
 Reference::g_old3dsAdpcmFast=false;Reference::g_old3dsAdpcmPrune=false;
#ifdef SRR2_OLD_3DS
 assert(Optimized::g_old3dsAdpcmFast);
#else
 assert(!Optimized::g_old3dsAdpcmFast);
#endif
 std::mt19937 rng(144);unsigned cases=0;uint64_t samples=0;
 for(unsigned pattern=0;pattern<9;++pattern)for(unsigned n=0;n<90;++n){
  unsigned count=n<8?14+n:n==8?44100:n==9?220500:14+rng()%8192;
  std::vector<s16> input(count+16);
  for(unsigned i=0;i<count;++i){
   int value=0;
   switch(pattern){
    case 0:value=int(rng()%65536)-32768;break;
    case 1:value=32767;break;case 2:value=-32768;break;
    case 3:value=i%2?-32768:32767;break;
    case 4:value=int(i%65536)-32768;break;
    case 5:value=int(30000*sin(i*.071));break;
    case 6:value=i==count/2?32767:0;break;
    case 7:value=int(rng()%32)-16;break;
    default:value=0;
   }
   input[i]=s16(value);
  }
  auto a=input,b=input,c=input;
  unsigned bytes=Reference::AdpcmEncodeInPlace(a.data(),count);
  Optimized::g_old3dsAdpcmPrune=false;Optimized::g_old3dsAdpcmFast=false;
  assert(Optimized::AdpcmEncodeInPlace(b.data(),count)==bytes);
  Optimized::g_old3dsAdpcmPrune=true;
  assert(Optimized::AdpcmEncodeInPlace(c.data(),count)==bytes);
  assert(a==b && a==c);
  auto fast=input;Optimized::g_old3dsAdpcmPrune=false;Optimized::g_old3dsAdpcmFast=true;
  const auto before=Optimized::g_old3dsAdpcmFastCalls;
  assert(Optimized::AdpcmEncodeInPlace(fast.data(),count)==bytes && fast==a);
  assert(Optimized::g_old3dsAdpcmFastCalls==before+1 && Optimized::g_old3dsAdpcmFastFallbacks==0);
  auto d=input;Optimized::g_old3dsAdpcmVerify=true;
  assert(Optimized::AdpcmEncodeInPlace(d.data(),count)==bytes && d==a);
  Optimized::g_old3dsAdpcmVerify=false;Optimized::g_old3dsAdpcmFast=false;
  assert(Optimized::g_old3dsAdpcmMismatches==0 && Optimized::g_old3dsAdpcmVerifyAllocFailures==0);++cases;samples+=count;
 }
 // The NDSP API takes a mutable coefficient pointer. Changed tables must
 // fall back to the runtime-table kernel, not hardcoded predictor constants.
 Reference::s_AdpcmCoefs[2]=2016;Optimized::s_AdpcmCoefs[2]=2016;
 std::vector<s16> input(8192);for(auto& value:input)value=s16(rng());
 auto a=input,b=input;Optimized::g_old3dsAdpcmFast=true;Optimized::g_old3dsAdpcmPrune=false;
 assert(Reference::AdpcmEncodeInPlace(a.data(),a.size())==Optimized::AdpcmEncodeInPlace(b.data(),b.size()));
 assert(a==b && Optimized::g_old3dsAdpcmFastFallbacks==1);
 printf("PASS: %u actual encoder cases / %llu PCM samples; original, false, pruned and fixed-predictor kernels match every byte, including untouched input tail. Full-range PCM, constants, silence, impulses, ramps, waveforms, partial frames and long clips.\n",cases,(unsigned long long)samples);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-adpcm-prune-') as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 # The original encoder left-shifts negative signed nibbles. Disable that
 # inherited shift check while retaining arithmetic, bounds and memory checks.
 for target in ([],['-DSRR2_OLD_3DS']):
  subprocess.run(['clang++','-std=c++14','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize=shift-base',*target,str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
