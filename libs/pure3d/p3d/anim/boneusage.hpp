#ifndef P3D_BONEUSAGE_HPP
#define P3D_BONEUSAGE_HPP
#include <cstdint>
#include <cstring>
namespace p3dSkin {
// Only palette pointers into this skin's shared matrix array are accepted.
// Palettes are fixed by the loader; positions, weights and GPU eligibility may
// change without changing which matrices the original palette can read.
class BoneUsage
{
    unsigned bits[8]={};
    unsigned joints=0;
public:
    void Begin(unsigned count)
    {
        std::memset(bits,0,sizeof(bits));
        joints=count>0 && count<256 ? count : 0;
    }
    void Invalidate(){joints=0;}
    bool Ready(unsigned count) const {return joints && count==joints;}
    bool Uses(unsigned index) const {return index<256 && (bits[index>>5] & (1u<<(index&31)));}
    bool Add(const void* base,const void* matrix)
    {
        const uintptr_t first=reinterpret_cast<uintptr_t>(base),value=reinterpret_cast<uintptr_t>(matrix);
        if(!joints || !base || !matrix || value<first || (value-first)%64 || (value-first)/64>joints)
        {Invalidate();return false;}
        const unsigned index=(value-first)/64;
        // The extra identity matrix is still written unconditionally by Display.
        if(index<joints) bits[index>>5]|=1u<<(index&31);
        return true;
    }
};
static_assert(sizeof(BoneUsage)==36,"Bounded skin bone usage metadata");
}
#endif
