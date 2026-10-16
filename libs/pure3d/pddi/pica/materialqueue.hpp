#ifndef PICA_MATERIAL_QUEUE_HPP
#define PICA_MATERIAL_QUEUE_HPP
#include <math.h>
#include <stdint.h>

// Independent of PDDI and libctru so the production scheduler can be tested
// under host sanitizers. No allocation, draw merging, or texture substitution.
namespace picaQueue
{
enum { Capacity = 32 };
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE_AB
extern bool enabled;
inline bool Enabled() { return enabled; }
#else
inline bool Enabled() { return true; }
#endif
struct Rect
{
    float low[2], high[2];
    bool valid;
};
struct Projection
{
    float matrix[3][4], error[3][4]; // clip x, y, w, including float24 error bounds
    bool valid;
};
inline Projection PrepareProjection(const float mv[4][4], const float projection[4][4])
{
    Projection result = {};
    for(unsigned a = 0; a < 3; ++a)
        for(unsigned b = 0; b < 4; ++b)
        {
            const unsigned row = a == 2 ? 3 : a;
            for(unsigned k = 0; k < 4; ++k)
            {
                const float r = projection[row][k], m = mv[k][b];
                if(!isfinite(r) || !isfinite(m) || fabsf(r) > 1.0e12f || fabsf(m) > 1.0e12f) return result;
                const float term = r*m;
                result.matrix[a][b] += term;
                // Conservative accumulated float24 input/multiply/add error for
                // both shader DP4 stages, including cancellation in world space.
                result.error[a][b] += fabsf(term) / 128.0f;
            }
        }
    result.valid = true;
    return result;
}
inline Rect ProjectBounds(const float* low, const float* high, const Projection& projection)
{
    Rect rect = {{0,0},{0,0},false};
    if(!projection.valid) return rect;
    float clipLow[3] = {}, clipHigh[3] = {};
    for(unsigned a = 0; a < 3; ++a)
    {
        float error = 1.0e-6f;
        for(unsigned b = 0; b < 4; ++b)
        {
            const float lo = b == 3 ? 1 : low[b], hi = b == 3 ? 1 : high[b];
            if(!isfinite(lo) || !isfinite(hi) || lo > hi || fabsf(lo) > 1.0e12f || fabsf(hi) > 1.0e12f) return rect;
            const float first = projection.matrix[a][b]*lo, last = projection.matrix[a][b]*hi;
            clipLow[a] += fminf(first,last);
            clipHigh[a] += fmaxf(first,last);
            error += projection.error[a][b]*fmaxf(fabsf(lo),fabsf(hi)) +
                     fmaxf(fabsf(first),fabsf(last))/1024.0f;
        }
        clipLow[a] -= error; clipHigh[a] += error;
        if(!isfinite(clipLow[a]) || !isfinite(clipHigh[a])) return rect;
    }
    if(!(clipLow[2] > 0.01f)) return rect;
    for(unsigned a = 0; a < 2; ++a)
    {
        rect.low[a] = 1.0e30f; rect.high[a] = -1.0e30f;
        for(unsigned edge = 0; edge < 4; ++edge)
        {
            const float value = ((edge & 1) ? clipHigh[a] : clipLow[a]) /
                                ((edge & 2) ? clipHigh[2] : clipLow[2]);
            if(!isfinite(value)) return rect;
            if(value < rect.low[a]) rect.low[a] = value;
            if(value > rect.high[a]) rect.high[a] = value;
        }
        // Two pixels at the smaller native viewport dimension for raster rounding.
        rect.low[a] -= 4.0f/240.0f; rect.high[a] += 4.0f/240.0f;
    }
    rect.valid = true;
    return rect;
}
inline Rect ProjectBounds(const float* low, const float* high,
                          const float mv[4][4], const float projection[4][4])
{
    return ProjectBounds(low, high, PrepareProjection(mv,projection));
}
inline bool Disjoint(const Rect& a, const Rect& b)
{
    // Invalid/near-plane bounds are dependencies, never permission to reorder.
    return a.valid && b.valid &&
        (a.high[0] < b.low[0] || b.high[0] < a.low[0] ||
         a.high[1] < b.low[1] || b.high[1] < a.low[1]);
}
template<class Submission>
bool HasMaterialOpportunity(const Submission* entries, unsigned count)
{
    int heads[32], next[Capacity];
    for(unsigned i = 0; i < 32; ++i) heads[i] = -1;
    for(unsigned i = 0; i < count; ++i)
    {
        const uintptr_t key = (uintptr_t)entries[i].material;
        const unsigned bucket = ((key >> 4) ^ (key >> 9)) & 31;
        if(i && entries[i].material != entries[i-1].material)
            for(int j = heads[bucket]; j >= 0; j = next[j])
                if(entries[i].material == entries[j].material && (unsigned)j + 1 < i) return true;
        next[i] = heads[bucket];
        heads[bucket] = i;
    }
    return false;
}

// Pull a matching material forward only across submissions with disjoint
// screen coverage. Every potentially overlapping pair retains original order,
// including equal-depth surfaces. This also preserves per-pixel overdraw order.
template<class Submission>
unsigned Schedule(Submission* entries, unsigned count)
{
    unsigned moved = 0;
    for(unsigned i = 1; i < count; ++i)
    {
        if(entries[i].material == entries[i-1].material) continue;
        for(unsigned j = i + 1; j < count; ++j)
        {
            if(entries[j].material != entries[i-1].material) continue;
            bool safe = true;
            for(unsigned k = i; k < j; ++k)
                if(!Disjoint(entries[j].rect, entries[k].rect)) { safe = false; break; }
            if(!safe) continue;
            const Submission selected = entries[j];
            for(unsigned k = j; k > i; --k) entries[k] = entries[k-1];
            entries[i] = selected;
            ++moved;
            break;
        }
    }
    return moved;
}
}
#endif
