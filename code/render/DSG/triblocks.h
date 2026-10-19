#ifndef SRR2_TRI_BLOCKS_H
#define SRR2_TRI_BLOCKS_H
#include <cmath>
#include <limits>
namespace TriBlocks
{
const unsigned Size = 8;
#ifdef SRR2_OLD3DS_TRI_HIERARCHY
const unsigned Budget = 80 * 1024;
#else
const unsigned Budget = 64 * 1024;
#endif
struct Bounds
{
    float minX, maxX, minZ, maxZ;
    void Reset()
    {
        minX = minZ = std::numeric_limits<float>::infinity();
        maxX = maxZ = -minX;
    }
    void Add(float x, float z)
    {
        if(!std::isfinite(x) || !std::isfinite(z))
        {
            // Invalid geometry must take the original per-vertex comparison path.
            minX = minZ = -std::numeric_limits<float>::infinity();
            maxX = maxZ = -minX;
            return;
        }
        if(x < minX) minX = x;
        if(x > maxX) maxX = x;
        if(z < minZ) minZ = z;
        if(z > maxZ) maxZ = z;
    }
    bool Reject(float xLow, float xHigh, float zLow, float zHigh) const
    {
        // Compare against the original rounded query +/- epsilon. Expanding the
        // bounds instead changes rounding at terrain edges and is not equivalent.
        return minX > xHigh || maxX < xLow || minZ > zHigh || maxZ < zLow;
    }
};
inline unsigned Count(unsigned triangles)
{
    const unsigned blocks = (triangles + Size - 1) / Size;
#ifdef SRR2_OLD3DS_TRI_HIERARCHY
    return blocks + (blocks + 7) / 8 + (blocks + 63) / 64;
#else
    return blocks;
#endif
}
#ifdef SRR2_OLD3DS_TRI_HIERARCHY
inline void BuildHierarchy(Bounds* bounds, unsigned triangles)
{
    const unsigned blocks = (triangles + Size - 1) / Size;
    const unsigned middleCount = (blocks + 7) / 8;
    // Leaves retain the original groups of eight. Parents cover contiguous
    // ranges, so descending traversal preserves every candidate and tie order.
    for(unsigned level = 0; level < 2; ++level)
    {
        const unsigned width = level ? 64 : 8;
        const unsigned count = (blocks + width - 1) / width;
        Bounds* output = bounds + blocks + (level ? middleCount : 0);
        for(unsigned group = 0; group < count; ++group)
        {
            Bounds merged = bounds[group * width];
            const unsigned end = (group + 1) * width < blocks ? (group + 1) * width : blocks;
            for(unsigned leaf = group * width + 1; leaf < end; ++leaf)
            {
                const Bounds& other = bounds[leaf];
                if(other.minX < merged.minX) merged.minX = other.minX;
                if(other.maxX > merged.maxX) merged.maxX = other.maxX;
                if(other.minZ < merged.minZ) merged.minZ = other.minZ;
                if(other.maxZ > merged.maxZ) merged.maxZ = other.maxZ;
            }
            output[group] = merged;
        }
    }
}
inline int Skip(const Bounds* bounds, unsigned triangles, int index, int initial,
                float xLow, float xHigh, float zLow, float zHigh)
{
    const unsigned blocks = (triangles + Size - 1) / Size;
    if((index == initial || (index & 511) == 511) &&
       bounds[blocks + (blocks + 7) / 8 + index / 512].Reject(xLow,xHigh,zLow,zHigh))
        return index & ~511;
    if((index == initial || (index & 63) == 63) &&
       bounds[blocks + index / 64].Reject(xLow,xHigh,zLow,zHigh))
        return index & ~63;
    if((index == initial || (index & 7) == 7) &&
       bounds[index / Size].Reject(xLow,xHigh,zLow,zHigh))
        return index & ~7;
    return -1;
}
#endif

}
#endif
