#!/usr/bin/env python3
"""Compare production quaternion interpolation with/without invariant reuse."""
from pathlib import Path
import subprocess,tempfile,sys
associative="--associative" in sys.argv
root=Path(__file__).resolve().parents[3]
s=(root/'libs/radmath/radmath/quaternion.cpp').read_text();a=s.index('void Quaternion::Slerp(');b=s.index('\n} // namespace',a);method=s[a:b]
harness=r'''
#pragma STDC FENV_ACCESS ON
#include <radmath/slerpcache.hpp>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <random>
#include <thread>
#include <limits>
std::atomic<unsigned> trigCalls{0};
float ACos(float x){++trigCalls;return acosf(x);}float Sin(float x){++trigCalls;return sinf(x);}
struct Quaternion {float w,x,y,z;void Slerp(const Quaternion&,const Quaternion&,float);};
constexpr float quatEpsilon=1.0f/360.0f;
'''
classes='struct Quaternion {float w,x,y,z;void Slerp(const Quaternion&,const Quaternion&,float);};\nconstexpr float quatEpsilon=1.0f/360.0f;\n'
tests=r'''
void compare(float* a,float* b,float t){
 Reference::Quaternion qa,qb,expected;Optimized::Quaternion ca,cb,actual;
 memcpy(&qa,a,16);memcpy(&qb,b,16);memcpy(&ca,a,16);memcpy(&cb,b,16);
 expected.Slerp(qa,qb,t);actual.Slerp(ca,cb,t);assert(!memcmp(&expected,&actual,16));
}
void exercise(unsigned seed){
 std::mt19937 rng(seed);std::uniform_real_distribution<float>d(-1,1);
 for(unsigned i=0;i<50000;++i){
  float a[4],b[4];for(unsigned j=0;j<4;++j){a[j]=d(rng);b[j]=d(rng);}
  // Normalized, sign-reversed, equal, nonunit, signed-zero and nonfinite inputs.
  if(i%6<3){for(auto* q:{a,b}){float n=sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);for(unsigned j=0;j<4;++j)q[j]/=n;}}
  if(i%6==1)for(unsigned j=0;j<4;++j)b[j]=-a[j];
  if(i%6==2)memcpy(b,a,16);
  if(i%6==3){a[i%4]=-0.0f;b[(i+1)%4]=0.0f;}
  if(i%6==4){uint32_t nan=0x7fc00000u+(i&0xffff);memcpy(&a[i%4],&nan,4);}
  if(i%6==5)b[i%4]=std::numeric_limits<float>::infinity();
  for(float t:{-.5f,0.0f,.1f,.5f,1.0f,1.5f})compare(a,b,t);
 }
}
int main(){
#ifdef SRR2_OLD3DS_SLERP_ASSOCIATIVE
 assert(!fesetround(FE_TONEAREST));
 // Three finite invariants competing for the same original direct-map slot.
 uint32_t keys[3]={};unsigned count=0,target=0;
 for(uint32_t bits=0x3e000001;count<3;++bits){
  unsigned slot=RadicalMathLibrary::slerpCache::Hash(bits)&255u;
  if(!count)target=slot;
  if(slot==target)keys[count++]=bits;
 }
 float values[3];for(unsigned i=0;i<3;++i)memcpy(values+i,keys+i,4);
 namespace cache=RadicalMathLibrary::slerpCache;
 cache::Store(values[0],.25f,2.f);cache::Store(values[1],.5f,3.f);
 float angle,inv;assert(cache::Find(values[0],angle,inv)&&angle==.25f&&inv==2.f);
 assert(cache::Find(values[1],angle,inv)&&angle==.5f&&inv==3.f);
 assert(cache::Find(values[0],angle,inv));
 cache::Store(values[2],.75f,4.f);
 assert(!cache::Find(values[1],angle,inv));
 assert(cache::Find(values[0],angle,inv));assert(cache::Find(values[2],angle,inv));
 // A duplicate store must retain the other way.
 cache::Store(values[0],.25f,2.f);assert(cache::Find(values[2],angle,inv));
 assert(sizeof(cache::entries)==4096&&sizeof(cache::victim)==128);
 assert(!fesetround(FE_UPWARD));assert(!cache::Find(values[0],angle,inv));
 assert(!fesetround(FE_TONEAREST));assert(cache::Find(values[0],angle,inv));
 puts("PASS: two colliding invariants coexist; LRU eviction, duplicate store, FP-control mismatch and 4224-byte bound");
 // Clear manufactured invariant outputs before testing production arithmetic.
 memset(cache::entries,0,sizeof(cache::entries));memset(cache::victim,0,sizeof(cache::victim));
#endif
 exercise(3921);
 for(int mode:{FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO,FE_TONEAREST}){
  assert(!fesetround(mode));float a[4]={1,0,0,0},b[4]={.5f,.5f,.5f,.5f};
  for(unsigned i=0;i<1000;++i)compare(a,b,float(i)/1000);
 }
 std::thread a([]{exercise(761);}),b([]{exercise(762);});a.join();b.join();
 Reference::Quaternion qa{1,0,0,0},qb{.5f,.5f,.5f,.5f},reference;
 Optimized::Quaternion ca{1,0,0,0},cb{.5f,.5f,.5f,.5f},optimized;
 trigCalls=0;for(unsigned i=0;i<10000;++i)reference.Slerp(qa,qb,float(i)/10000);unsigned original=trigCalls;
 trigCalls=0;for(unsigned i=0;i<10000;++i)optimized.Slerp(ca,cb,float(i)/10000);unsigned cached=trigCalls;
 assert(cached<original&&cached>=20000);printf("PASS: 904,000 production Slerp comparisons; rounding controls, NaN/Inf, extrapolation, concurrent access; repeated-pair trig calls %u -> %u\n",original,cached);
}
'''
with tempfile.TemporaryDirectory(prefix='srr-slerp-cache-')as td:
 p=Path(td);code=('#define SRR2_OLD3DS_SLERP_ASSOCIATIVE\n' if associative else '')+harness.split('struct Quaternion')[0]+'namespace Reference{\n'+classes+method+'}\n#define SRR2_OLD3DS_SLERP_CACHE\nnamespace Optimized{\nusing RadicalMathLibrary::slerpCache::Find;\nnamespace slerpCache=RadicalMathLibrary::slerpCache;\n'+classes+method+'}\n'+tests;(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++14','-O2','-ffp-contract=off','-pthread','-fsanitize=address,undefined','-I',str(root/'libs/radmath'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
