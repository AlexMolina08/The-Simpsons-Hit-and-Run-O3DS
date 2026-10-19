#ifndef PICA_PARTICLE_TRIG_CACHE_HPP
#define PICA_PARTICLE_TRIG_CACHE_HPP
#include <cstdint>
#include <cstring>
#include <cerrno>
#ifndef __arm__
#include <cfenv>
#endif
namespace picaParticleTrig {
inline unsigned State()
{
#ifdef __arm__
    unsigned state;
    __asm__ __volatile__("vmrs %0, fpscr":"=r"(state)::"memory");
    return state;
#else
    return (unsigned(std::fegetround())<<8)|unsigned(std::fetestexcept(FE_ALL_EXCEPT));
#endif
}
inline unsigned Bits(float value)
{
    unsigned bits;std::memcpy(&bits,&value,4);return bits;
}
// One emitter display owns this value-only cache. Retain the full post-call
// floating-point state and errno: a hit already has the same observable state
// that another call would leave. Changed flags/modes force the original call.
class Cache {
    unsigned angleBits=0,state=0;
    int error=0;
    float sine=0,cosine=0;
    bool valid=false;
public:
    template<class Function> bool Compute(float angle,float* s,float* c,Function original)
    {
        const unsigned input=Bits(angle),current=State();
#ifdef __arm__
        const bool enabledExceptions=(current&0x00009f00u)!=0;
#else
        const bool enabledExceptions=false;
#endif
        if(!enabledExceptions && s!=c && valid && input==angleBits && current==state && errno==error)
        {
            *s=sine;*c=cosine;return true;
        }
        original(angle,s,c);
        valid=!enabledExceptions && s!=c && (input&0x7f800000u)!=0x7f800000u &&
            (Bits(*s)&0x7f800000u)!=0x7f800000u && (Bits(*c)&0x7f800000u)!=0x7f800000u;
        if(valid)
        {
            sine=*s;cosine=*c;angleBits=input;
            state=State();error=errno;
        }
        return false;
    }
};
static_assert(sizeof(Cache)<=32,"Bounded particle trigonometry cache");
// Render-thread ownership only. A fixed table can retain stable particle angles
// across emitter displays without retaining particles, assets or source pointers.
class Table {
    Cache entries[512];
public:
    template<class Function> bool Compute(float angle,float* s,float* c,Function original)
    {
        const unsigned bits=Bits(angle);
        const unsigned slot=((bits^(bits>>11))*2654435761u)>>23;
        return entries[slot].Compute(angle,s,c,original);
    }
};
static_assert(sizeof(Table)<=16*1024,"Bounded shared particle trigonometry cache");
}
#endif
