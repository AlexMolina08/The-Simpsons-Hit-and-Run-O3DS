#ifndef OLD3DS_SPHERE_WALK_CACHE_HPP
#define OLD3DS_SPHERE_WALK_CACHE_HPP
#include <stdint.h>
#include <string.h>
#ifdef SRR2_OLD3DS_SPHERE_REGION_REUSE
#include <limits>
#endif
#ifndef __arm__
#include <cfenv>
#endif

// Only tree-node indices survive a query. Entity lists, bounds and collision
// results are deliberately not retained. SetToRoot/Clear invalidate all entries.
class Old3dsSphereWalkCache
{
public:
    struct Counters
    {
        unsigned queries, hits, replayedNodes, oversized, fallback;
#ifdef SRR2_OLD3DS_SPHERE_REGION_REUSE
        unsigned regionHits;
#endif
    };
    static Counters& Metrics()
    {
        static Counters counters={};
        return counters;
    }
    struct Entry
    {
        uint32_t key[6];
        uint16_t nodes[64];
        unsigned count;
        bool valid;
    };
    Old3dsSphereWalkCache() { Clear(); }
    void Clear()
    {
        for(unsigned i=0; i<32; ++i) entries[i].valid=false;
#ifdef SRR2_OLD3DS_SPHERE_REGION_REUSE
        for(unsigned i=0; i<4; ++i) regions[i].valid=false;
        nextRegion=0;
#endif
    }
    static bool ScalarControls()
    {
#ifdef __arm__
        unsigned control;
        __asm__ volatile("vmrs %0, fpscr" : "=r"(control));
        return (control & 0x03f79f00u) == 0x03000000u;
#else
        return std::fegetround() == FE_TONEAREST;
#endif
    }
    static bool MakeKey(const float bounds[6], uint32_t key[6])
    {
        memcpy(key,bounds,6*sizeof(float));
        for(unsigned i=0; i<6; ++i)
            if((key[i]&0x7f800000u)==0x7f800000u) return false;
        return true;
    }
    const Entry* Find(const uint32_t key[6]) const
    {
        const Entry& entry=entries[Hash(key)];
        return entry.valid && memcmp(entry.key,key,sizeof(entry.key))==0 ? &entry : 0;
    }
    Entry* Begin(const uint32_t key[6])
    {
        Entry& entry=entries[Hash(key)];
        entry.valid=false;
        entry.count=0;
#ifdef SRR2_OLD3DS_SPHERE_REGION_REUSE
        pending.valid=true;
        for(unsigned axis=0; axis<3; ++axis)
        {
            pending.limits[axis][0]=-std::numeric_limits<float>::infinity();
            pending.limits[axis][1]= std::numeric_limits<float>::infinity();
            pending.limits[axis][2]=-std::numeric_limits<float>::infinity();
            pending.limits[axis][3]= std::numeric_limits<float>::infinity();
        }
#endif
        memcpy(entry.key,key,sizeof(entry.key));
        return &entry;
    }
    static void Record(Entry* entry, unsigned node)
    {
        if(entry && entry->count<=64)
        {
            if(entry->count<64) entry->nodes[entry->count]=static_cast<uint16_t>(node);
            ++entry->count;
        }
    }
    void Commit(Entry* entry)
    {
        entry->valid=entry->count<=64;
#ifdef SRR2_OLD3DS_SPHERE_REGION_REUSE
        if(entry->valid && pending.valid)
        {
            Region& region=regions[nextRegion];
            memcpy(region.limits,pending.limits,sizeof(region.limits));
            region.selection.count=entry->count;
            region.selection.valid=true;
            memcpy(region.selection.nodes,entry->nodes,entry->count*sizeof(entry->nodes[0]));
            region.valid=true;
            nextRegion=(nextRegion+1)&3u;
        }
#endif
    }
#ifdef SRR2_OLD3DS_SPHERE_REGION_REUSE
    // Bounds may change only while every visited split makes the same decision.
    // Upper < plane wins before lower > plane, including negative-radius queries.
    void RecordPlane(unsigned axis, float plane, float side)
    {
        uint32_t bits; memcpy(&bits,&plane,sizeof(bits));
        if((bits&0x7f800000u)==0x7f800000u) { pending.valid=false; return; }
        float* limits=pending.limits[axis];
        if(side>0)
        {
            if(plane<limits[3]) limits[3]=plane;
        }
        else
        {
            if(plane>limits[2]) limits[2]=plane;
            if(side<0) { if(plane>limits[0]) limits[0]=plane; }
            else { if(plane<limits[1]) limits[1]=plane; }
        }
    }
    const Entry* FindRegion(const float bounds[6]) const
    {
        for(unsigned i=0; i<4; ++i)
        {
            const Region& region=regions[(nextRegion+3-i)&3u];
            if(!region.valid) continue;
            unsigned axis=0;
            for(; axis<3; ++axis)
            {
                const float* limits=region.limits[axis];
                if(!(bounds[axis]>limits[0] && bounds[axis]<=limits[1] &&
                     bounds[axis+3]>=limits[2] && bounds[axis+3]<limits[3])) break;
            }
            if(axis==3) return &region.selection;
        }
        return 0;
    }
#endif
private:
    static unsigned Hash(const uint32_t key[6])
    {
        uint32_t value=0x9e3779b9u;
        for(unsigned i=0; i<6; ++i) value=(value^key[i])*0x85ebca6bu;
        value^=value>>16;
        return value&31u;
    }
    Entry entries[32];
#ifdef SRR2_OLD3DS_SPHERE_REGION_REUSE
    struct RegionBounds
    {
        float limits[3][4];
        bool valid;
    };
    struct Region : RegionBounds { Entry selection; };
    Region regions[4];
    RegionBounds pending;
    unsigned nextRegion;
#endif
};
#endif
