#!/usr/bin/env python3
"""Verify exact repeated-input trig reuse and unchanged particle corners."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
SOURCE = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
#include "pddi/pica/particletrigcache.hpp"
unsigned calls=0;
float (*volatile originalSin)(float)=::sinf;
float (*volatile originalCos)(float)=::cosf;
__attribute__((noinline)) void Calculate(float angle,float* sine,float* cosine){++calls;*sine=originalSin(angle);*cosine=originalCos(angle);}
unsigned Bits(float value){unsigned bits;memcpy(&bits,&value,4);return bits;}
int main(){
    const unsigned ordinary=0x03000000u;
    picaParticleTrig::Cache cache;
    float sine,cosine;
    cache.Compute(.7853982f,ordinary,&sine,&cosine,Calculate);
    const unsigned s=Bits(sine),c=Bits(cosine);
    for(unsigned i=0;i<1000;++i){cache.Compute(.7853982f,ordinary,&sine,&cosine,Calculate);assert(Bits(sine)==s&&Bits(cosine)==c);}
    assert(calls==1&&cache.requests==1001&&cache.evaluations==1);
    // Rounding modes, enabled traps, large values and nonfinite inputs retain
    // every original evaluation. Sticky exception flags do not change controls.
    for(unsigned control:{0u,0x03400000u,ordinary|0x100u,ordinary|0x8000u}){
        const unsigned before=calls;
        for(unsigned i=0;i<4;++i)cache.Compute(.7853982f,control,&sine,&cosine,Calculate);
        assert(calls==before+4&&!cache.ready);
    }
    for(float angle:{8192.01f,-8192.01f,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        const unsigned before=calls;
        for(unsigned i=0;i<4;++i)cache.Compute(angle,ordinary,&sine,&cosine,Calculate);
        assert(calls==before+4&&!cache.ready);
    }
    unsigned seed=93;cache={};unsigned matches=0;
    for(unsigned i=0;i<100000;++i){
        seed=seed*1664525u+1013904223u;
        const float angle=i%7<4?.7853982f:float(int(seed%200000)-100000)/8192.f;
        const unsigned control=i%37?ordinary:0u;
        float a,b,x,y;Calculate(angle,&a,&b);const unsigned before=calls;
        cache.Compute(angle,control,&x,&y,Calculate);
        assert(Bits(a)==Bits(x)&&Bits(b)==Bits(y));matches+=calls==before;
        const float size=float(seed%97)/17.f;
        a*=size;b*=size;x*=size;y*=size;
        const float center[3]={float(int(seed%10000)-5000),12.5f,-43.25f};
        const float expected[12]={center[0]+b,center[1]+a,center[2],center[0]-b,center[1]-a,center[2],center[0]-a,center[1]+b,center[2],center[0]+a,center[1]-b,center[2]};
        const float actual[12]={center[0]+y,center[1]+x,center[2],center[0]-y,center[1]-x,center[2],center[0]-x,center[1]+y,center[2],center[0]+x,center[1]-y,center[2]};
        assert(!memcmp(expected,actual,sizeof(expected)));
    }
    assert(matches>30000&&cache.evaluations<cache.requests);
    // Fresh emitter storage never inherits another emitter's cached result.
    picaParticleTrig::Cache other;const unsigned before=calls;
    other.Compute(.7853982f,ordinary,&sine,&cosine,Calculate);assert(calls==before+1);
    cache={};cache.Compute(0.f,ordinary,&sine,&cosine,Calculate);
    const unsigned zero=calls;cache.Compute(-0.f,ordinary,&sine,&cosine,Calculate);
    assert(calls==zero+1);
}
'''
particle = (ROOT / 'libs/pure3d/p3d/effects/particleemitter.cpp').read_text()
a = particle.index('void tSpriteEmitter::Display()')
a = particle.index('            float x, y;', a)
b = particle.index('            y *= current->size;', a) + len('            y *= current->size;')
branch = particle[a:b].replace('__asm__ __volatile__("vmrs %0, fpscr":"=r"(control)::"memory");', 'control=selectedControl;')
wrapper = r'''namespace rmt {
constexpr float PI_BY4=.78539816339744830962f;
void SinCos(float angle,float* sine,float* cosine){Calculate(angle,sine,cosine);}
}
struct Particle {float spin,size;};
void Production(const Particle* current,bool cacheParticleTrig,picaParticleTrig::Cache& particleTrig,unsigned selectedControl,float* output){
BRANCH
output[0]=x;output[1]=y;
}
'''.replace('BRANCH',branch)
SOURCE=SOURCE.replace('int main(){',wrapper+'\nint main(){')
SOURCE=SOURCE.replace('    assert(matches>30000',r'''    picaParticleTrig::Cache production;
    for(unsigned i=0;i<2000;++i){
        Particle particle{i%5?0.f:float(i)/128.f,float(i%17+1)/3};
        const unsigned control=i%11?ordinary:0u;
        float original[2],cached[2];
        Production(&particle,false,production,control,original);
        Production(&particle,true,production,control,cached);
        assert(!memcmp(original,cached,sizeof(original)));
    }
    assert(production.requests==2000&&production.evaluations<production.requests);
    assert(matches>30000''')
with tempfile.TemporaryDirectory(prefix='srr-particle-trig-') as temporary:
    folder = Path(temporary)
    source = folder / 'test.cpp'
    source.write_text(SOURCE)
    subprocess.run(['clang++', '-std=c++17', '-O2', '-g',
                    '-DSRR2_OLD3DS_NATIVE_BENCH', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I', str(ROOT / 'libs/pure3d'), str(source),
                    '-o', str(folder / 'test')], check=True)
    subprocess.run([str(folder / 'test')], check=True)
print('PASS: exact trig outputs and particle corners for 100000 inputs, bounded normal controls, original rare-input evaluations and per-emitter isolation.')
