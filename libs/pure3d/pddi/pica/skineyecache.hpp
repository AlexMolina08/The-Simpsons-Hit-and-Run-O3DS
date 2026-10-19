#ifndef PICA_SKINEYECACHE_HPP
#define PICA_SKINEYECACHE_HPP
#include <pddi/pica/skincore.hpp>
namespace picaSkin
{
class EyeCache
{
    struct Entry
    {
        float source[4][4], packed[3][4];
        unsigned generation;
    };
    Entry entries[64] = {};
    float view[4][4] = {};
    unsigned generation = 0, control = 0;
    bool seen = false, valid = false;
public:
    bool PrepareView(const float input[4][4], unsigned inputControl)
    {
        if(!seen || control != inputControl || std::memcmp(view, input, sizeof(view)) != 0)
        {
            seen = true;
            control = inputControl;
            std::memcpy(view, input, sizeof(view));
            valid = EyeViewValid(view);
            if(++generation == 0)
            {
                for(unsigned i = 0; i < 64; ++i) entries[i].generation = 0;
                generation = 1;
            }
        }
        return valid;
    }
    bool Pack(float output[3][4], const float source[4][4], std::uintptr_t key)
    {
        if(!valid) return false;
        // Shared bone matrices are contiguous 64-byte records. Hashing retains
        // no asset pointer; complete value comparison handles aliases/mutations.
        Entry& entry = entries[(key >> 6) & 63u];
        if(entry.generation != generation || std::memcmp(entry.source, source, sizeof(entry.source)) != 0)
        {
            if(!PackEyeMatrixValidatedView(output, source, view)) return false;
            std::memcpy(entry.source, source, sizeof(entry.source));
            std::memcpy(entry.packed, output, sizeof(entry.packed));
            entry.generation = generation;
        }
        else
            std::memcpy(output, entry.packed, sizeof(entry.packed));
        return true;
    }
};
static_assert(sizeof(EyeCache) <= 8192, "Bounded eye-palette cache");
}
#endif
