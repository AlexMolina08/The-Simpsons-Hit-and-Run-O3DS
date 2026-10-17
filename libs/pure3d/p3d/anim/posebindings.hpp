#ifndef P3D_OLD3DS_POSE_BINDINGS_HPP
#define P3D_OLD3DS_POSE_BINDINGS_HPP

#include <stdint.h>

// One blend call owns this scratch. Its animation slots and joint UIDs cannot
// change between the translation and rotation passes. No asset pointers survive
// the call, so unloading, cloning and retargeting need no invalidation machinery.
template<class Animation, class Group>
class Old3dsPoseBindings
{
public:
    Old3dsPoseBindings()
    {
        for(unsigned i = 0; i < 4; ++i) valid[i] = 0;
    }

    template<class UID>
    const Group* Get(const Animation* animation, unsigned slot, unsigned joint, UID uid)
    {
        if(slot >= 4 || joint >= 64) return animation->GetGroupByUID(uid);
        const uint64_t bit = uint64_t(1) << joint;
        if(!(valid[slot] & bit))
        {
            groups[slot][joint] = animation->GetGroupByUID(uid);
            valid[slot] |= bit;
        }
        return groups[slot][joint];
    }

private:
    const Group* groups[4][64];
    uint64_t valid[4];
};

#endif
