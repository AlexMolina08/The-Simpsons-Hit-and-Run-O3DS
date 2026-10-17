#ifndef OLD3DS_SLOT_INDEX_HPP
#define OLD3DS_SLOT_INDEX_HPP
#include <stdint.h>
#include <string.h>

// Callback-local index. Entries only gain objects during submission; removals
// happen afterwards. First occurrence wins, matching the original slot scan.
// Small queries and unsupported list sizes retain the original scan.
template<class Pointer>
class Old3dsSlotIndex
{
public:
    explicit Old3dsSlotIndex(bool enabled) : enabled(enabled)
    {
        if(enabled) memset(slots, 255, sizeof(slots));
    }
    bool Enabled() const { return enabled; }
    void Insert(Pointer pointer, unsigned index)
    {
        if(index >= 64) { enabled = false; return; }
        if(!enabled || !pointer) return;
        unsigned bucket = Hash(pointer);
        while(slots[bucket] != 255)
        {
            if(keys[bucket] == pointer) return;
            bucket = (bucket + 1) & 127;
        }
        keys[bucket] = pointer;
        slots[bucket] = static_cast<unsigned char>(index);
    }
    int Find(Pointer pointer) const
    {
        if(!enabled || !pointer) return -1;
        unsigned bucket = Hash(pointer);
        while(slots[bucket] != 255)
        {
            if(keys[bucket] == pointer) return slots[bucket];
            bucket = (bucket + 1) & 127;
        }
        return -1;
    }
private:
    static unsigned Hash(Pointer pointer)
    {
        uintptr_t value = reinterpret_cast<uintptr_t>(pointer);
        return ((value >> 4) ^ (value >> 11)) & 127;
    }
    Pointer keys[128];
    unsigned char slots[128];
    bool enabled;
};
#endif
