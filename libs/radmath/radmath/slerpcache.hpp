#ifndef RADMATH_SLERPCACHE_HPP
#define RADMATH_SLERPCACHE_HPP
#include <atomic>
#include <cstdint>
#include <cstring>
#ifndef __arm__
#include <cfenv>
#endif
namespace RadicalMathLibrary
{
namespace slerpCache
{
struct Entry
{
    std::uint32_t input;
    float angle, inverseSine;
    std::uint32_t control;
};
// Fixed storage, no channel/object growth, allocations or retained asset pointers.
static Entry entries[256] = {};
#ifdef SRR2_OLD3DS_SLERP_ASSOCIATIVE
// Two ways retain competing angle invariants without growing the entry pool.
// One byte per set selects the least recently used way; the guard protects it.
static unsigned char victim[128] = {};
#endif
static std::atomic_flag busy = ATOMIC_FLAG_INIT;
static unsigned hits = 0, misses = 0;
struct Guard
{
    bool held;
    Guard() : held(!busy.test_and_set(std::memory_order_acquire)) {}
    ~Guard() { if(held) busy.clear(std::memory_order_release); }
};
static_assert(sizeof(entries) == 4096, "Bounded exact Slerp invariant cache");
inline std::uint32_t Control()
{
#ifdef __arm__
    std::uint32_t control;
    __asm__ __volatile__("vmrs %0, fpscr" : "=r"(control) : : "memory");
    // Rounding, flush-to-zero and default-NaN controls affect arithmetic results.
    return (control & 0x03c00000u) | 1u;
#else
    return (static_cast<std::uint32_t>(std::fegetround()) << 1) | 1u;
#endif
}
inline unsigned Hash(std::uint32_t bits)
{
    return bits ^ (bits >> 11) ^ (bits >> 19);
}
inline Entry& Slot(std::uint32_t bits)
{
    return entries[Hash(bits) & 255u];
}
inline bool Find(float input, float& angle, float& inverseSine)
{
    std::uint32_t bits;
    std::memcpy(&bits, &input, sizeof(bits));
    Guard guard;
    if(!guard.held) return false;
#ifdef SRR2_OLD3DS_SLERP_ASSOCIATIVE
    const unsigned set = Hash(bits) & 127u;
    const std::uint32_t control = Control();
    for(unsigned way=0;way<2;++way)
    {
        const Entry& entry = entries[set*2+way];
        if(entry.control != control || entry.input != bits) continue;
        victim[set] = static_cast<unsigned char>(1-way);
        ++hits;
        angle = entry.angle;
        inverseSine = entry.inverseSine;
        return true;
    }
    ++misses;
    return false;
#else
    const Entry& entry = Slot(bits);
    if(entry.control != Control() || entry.input != bits) { ++misses; return false; }
    ++hits;
    angle = entry.angle;
    inverseSine = entry.inverseSine;
    return true;
#endif
}
inline bool ReadStats(unsigned* hitCount, unsigned* missCount)
{
    Guard guard;
    if(!guard.held) return false;
    *hitCount = hits;
    *missCount = misses;
    return true;
}
inline void Store(float input, float angle, float inverseSine)
{
    std::uint32_t bits;
    std::memcpy(&bits, &input, sizeof(bits));
    // Nonfinite inputs retain the original library calls and NaN propagation.
    if((bits & 0x7f800000u) == 0x7f800000u) return;
    Guard guard;
    if(!guard.held) return;
#ifdef SRR2_OLD3DS_SLERP_ASSOCIATIVE
    const unsigned set = Hash(bits) & 127u;
    const std::uint32_t control = Control();
    unsigned way = victim[set];
    // Another thread may have populated the same invariant between Find and
    // Store. Replace that entry instead of occupying both ways with one key.
    for(unsigned candidate=0;candidate<2;++candidate)
        if(entries[set*2+candidate].control == control && entries[set*2+candidate].input == bits)
        { way = candidate; break; }
    Entry& entry = entries[set*2+way];
    victim[set] = static_cast<unsigned char>(1-way);
#else
    Entry& entry = Slot(bits);
#endif
    entry.input = bits;
    entry.angle = angle;
    entry.inverseSine = inverseSine;
    entry.control = Control();
}
}
}
#endif
