#ifndef PICA_PARTICLE_TRIG_CACHE_HPP
#define PICA_PARTICLE_TRIG_CACHE_HPP
namespace picaParticleTrig {
static_assert(sizeof(float)==4 && sizeof(unsigned)==4,"Particle trig cache keys require 32-bit words");
// One emitter draw owns this cache. Reuse only identical bounded inputs with
// the same ordinary FP controls accepted by the existing combined trig kernel.
struct Cache {
    bool ready=false;
    unsigned key=0,requests=0,evaluations=0;
    float sine=0,cosine=0;
    template<class Calculate> void Compute(float angle,unsigned control,
        float* outSine,float* outCosine,Calculate calculate)
    {
        ++requests;
        unsigned bits;__builtin_memcpy(&bits,&angle,sizeof(bits));
        const bool ordinary=(control&0x03f79f00u)==0x03000000u &&
            (bits&0x7fffffffu)<=0x46000000u;
        if(!ordinary || !ready || bits!=key){
            ++evaluations;
            calculate(angle,outSine,outCosine);
            if(ordinary){key=bits;sine=*outSine;cosine=*outCosine;ready=true;}
            else ready=false;
            return;
        }
        *outSine=sine;*outCosine=cosine;
    }
};
}
#endif
