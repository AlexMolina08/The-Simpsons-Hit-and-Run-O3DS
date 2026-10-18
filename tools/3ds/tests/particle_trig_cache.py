#!/usr/bin/env python3
"""Check exact cached trig outputs, flags and errno under changing state."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=r'''
#pragma STDC FENV_ACCESS ON
#include <pddi/pica/particletrigcache.hpp>
#include <cmath>
#include <cstdio>
#include <cassert>
#include <initializer_list>
#include <limits>
static unsigned calls;
__attribute__((noinline)) void Original(float a,float*s,float*c){++calls;*s=sinf(a);*c=cosf(a);}
int main(){unsigned checks=0,hits=0;float angles[]={.785398185f,.785398185f,.785398185f,0.f,-0.f,1e-30f,1e-30f,1.f,1.f,-8192.f,-8192.f,1e30f,1e30f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()};
for(int round:{FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){
 assert(!fesetround(round));picaParticleTrig::Table cache;
 for(unsigned n=0;n<60000;++n){float a=angles[n%15],s,c,x,y;fenv_t before,after;int err=n%43?0:ERANGE;errno=err;
 if(n%79==0)feclearexcept(FE_ALL_EXCEPT);if(n%83==0)feraiseexcept(FE_DIVBYZERO);
 fegetenv(&before);Original(a,&s,&c);int expectedFlags=fetestexcept(FE_ALL_EXCEPT),expectedError=errno;fegetenv(&after);
 fesetenv(&before);errno=err;unsigned previous=calls;bool hit=cache.Compute(a,&x,&y,Original);assert(calls==previous+!hit);hits+=hit;
 assert(!memcmp(&s,&x,4)&&!memcmp(&c,&y,4));assert(fetestexcept(FE_ALL_EXCEPT)==expectedFlags&&errno==expectedError);fesetenv(&after);++checks;
 if(n%101==0){float z,v;Original(a,&z,&z);cache.Compute(a,&v,&v,Original);assert(!memcmp(&z,&v,4));}
 }
}
assert(hits>20000);printf("PASS: %u exact sine/cosine output, flags and errno comparisons; %u hits, four rounding modes, repeated/changing angles, cleared/raised flags, nonfinite fallback and aliased outputs. Not native timing.\n",checks,hits);}
'''
with tempfile.TemporaryDirectory(prefix='srr-particle-trig-') as d:
 p=Path(d);(p/'test.cpp').write_text(s)
 subprocess.run(['clang++','-std=c++17','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'libs/pure3d'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
