#ifndef PICA_SKINCORE_HPP
#define PICA_SKINCORE_HPP
#include <cmath>
#include <cstdint>
#include <cstring>
namespace picaSkin
{
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
const unsigned MaxBones = 25;
#elif defined(SRR2_OLD3DS_GPU_SKIN_COMPACT)
const unsigned MaxBones = 24;
#else
const unsigned MaxBones = 23;
#endif
const unsigned VertexStride = 56;
const unsigned Budget = 64 * 1024;
const unsigned Reserve = 2 * 1024 * 1024;
struct Source
{
    float weights[3];
    unsigned char indices[4];
    float normal[3], position[3];
};
struct Vertex
{
    float position[3], normal[3], uv[2];
    unsigned char colour[4];
    float weights[4];
    signed char offsets[4];
};
static_assert(sizeof(Source) == 40, "Pure3D source layout");
static_assert(sizeof(Vertex) == VertexStride, "GPU skin layout");
struct RigidVertex
{
    float position[3], normal[3], uv[2];
    unsigned char colour[4];
    signed char ignoredWeights[4], offsets[4];
};
static_assert(sizeof(RigidVertex) == 44, "Rigid GPU skin layout");
#ifdef SRR2_OLD3DS_GPU_SKIN_PACKED
struct PackedVertex
{
    std::int16_t position[3], normal[3], uv[2];
    unsigned char colour[4];
    float weights[4];
    signed char offsets[4];
};
struct PackedRigidVertex
{
    std::int16_t position[3], normal[3], uv[2];
    unsigned char colour[4];
    signed char ignoredWeights[4], offsets[4];
};
static_assert(sizeof(PackedVertex) == 40 && sizeof(PackedRigidVertex) == 28,
              "Packed skin attribute layout");
inline bool CanPack(const Vertex& vertex)
{
    for(unsigned i = 0; i < 3; ++i)
        if(!std::isfinite(vertex.position[i]) || std::fabs(vertex.position[i]) > 4.0f ||
           !std::isfinite(vertex.normal[i]) || std::fabs(vertex.normal[i]) > 1.0f) return false;
    for(unsigned i = 0; i < 2; ++i)
        if(!std::isfinite(vertex.uv[i]) || std::fabs(vertex.uv[i]) > 8.0f) return false;
    return true;
}
inline std::int16_t Quantize(float value, float multiplier)
{
    const float scaled = value * multiplier;
    int rounded = static_cast<int>(scaled + (scaled < 0 ? -0.5f : 0.5f));
    if(rounded > 32767) rounded = 32767;
    if(rounded < -32768) rounded = -32768;
    return static_cast<std::int16_t>(rounded);
}
inline void StorePackedVertex(void* destination, const Vertex& vertex, bool rigid)
{
    PackedVertex packed;
    for(unsigned i = 0; i < 3; ++i)
    {
        packed.position[i] = Quantize(vertex.position[i], 8192.0f);
        packed.normal[i] = Quantize(vertex.normal[i], 32768.0f);
    }
    for(unsigned i = 0; i < 2; ++i) packed.uv[i] = Quantize(vertex.uv[i], 4096.0f);
    std::memcpy(packed.colour, vertex.colour, 4);
    if(rigid)
    {
        PackedRigidVertex result;
        std::memcpy(&result, &packed, 20);
        std::memset(result.ignoredWeights, 0, 4);
        std::memcpy(result.offsets, vertex.offsets, 4);
        std::memcpy(destination, &result, sizeof(result));
    }
    else
    {
        std::memcpy(packed.weights, vertex.weights, sizeof(packed.weights));
        std::memcpy(packed.offsets, vertex.offsets, 4);
        std::memcpy(destination, &packed, sizeof(packed));
    }
}
#endif
inline unsigned PackedStride(bool rigid)
{
#ifdef SRR2_OLD3DS_GPU_SKIN_PACKED
    return rigid ? sizeof(PackedRigidVertex) : sizeof(PackedVertex);
#else
    return rigid ? sizeof(RigidVertex) : sizeof(Vertex);
#endif
}
inline void StoreVertex(void* dst, const Vertex& vertex, bool rigid)
{
#ifdef SRR2_OLD3DS_GPU_SKIN_PACKED
    StorePackedVertex(dst, vertex, rigid);
    return;
#endif
    if(rigid)
    {
        RigidVertex packed;
        std::memcpy(&packed, &vertex, 36);
        std::memset(packed.ignoredWeights, 0, sizeof(packed.ignoredWeights));
        std::memcpy(packed.offsets, vertex.offsets, sizeof(packed.offsets));
        std::memcpy(dst, &packed, sizeof(packed));
    }
    else
        std::memcpy(dst, &vertex, sizeof(vertex));
}
enum Reason
{
    Ready,
    NotSelected,
    Palette,
    Topology,
    SourceInvalid,
    Mutated,
    BudgetLimit,
    Allocation,
    Disabled,
    MatrixInvalid,
    NoShader,
    NoFrame,
    Count
};
inline bool Fits(unsigned bytes, unsigned live, unsigned freeBytes)
{
    return bytes <= Budget && live <= Budget - bytes && freeBytes >= bytes &&
           freeBytes - bytes >= Reserve;
}
#ifdef SRR2_OLD3DS_GPU_SKIN_BORROW
#ifdef SRR2_OLD3DS_GPU_SKIN_EXTRA_BUDGET
// VRAM texture promotion releases linear memory. Spend at most another 128 KiB
// on immutable skin buffers; allocation still retains the original free reserve.
const unsigned SharedBudget = 5 * Budget;
const unsigned BorrowPoolLimit = Budget + Budget + Budget / 2;
#else
const unsigned SharedBudget = 3 * Budget;
const unsigned BorrowPoolLimit = Budget + Budget / 2;
#endif
inline bool FitsBorrowed(unsigned bytes, unsigned poolLive, unsigned allLive, unsigned freeBytes)
{
    // Live counts include retired buffers until FrameBegin's GPU fence completes.
    return bytes <= Budget && poolLive <= BorrowPoolLimit - bytes &&
           allLive <= SharedBudget - bytes && freeBytes >= bytes &&
           freeBytes - bytes >= Reserve;
}
#endif
inline bool PackVertex(Vertex &dst, const Source &src, const void *original, unsigned bones)
{
    if (!bones || bones > MaxBones)
        return false;
    for (unsigned i = 0; i < 3; ++i)
        if (!std::isfinite(src.position[i]) || !std::isfinite(src.normal[i]) ||
            !std::isfinite(src.weights[i]))
            return false;
    const bool rigid = src.weights[0] == 1.0f;
    for (unsigned i = 0; i < (rigid ? 1u : 4u); ++i)
        if (src.indices[i] >= bones)
            return false;
    std::memcpy(&dst, original, 36);
    std::memcpy(dst.position, src.position, 12);
    std::memcpy(dst.normal, src.normal, 12);
    dst.weights[0] = src.weights[0];
    dst.weights[1] = rigid ? 0.0f : src.weights[1];
    dst.weights[2] = rigid ? 0.0f : src.weights[2];
    dst.weights[3] = rigid ? 0.0f : 1.0f - (src.weights[2] + src.weights[1] + src.weights[0]);
    if (!std::isfinite(dst.weights[3]))
        return false;
    for (unsigned i = 0; i < 4; ++i)
        dst.offsets[i] = static_cast<signed char>(3 * src.indices[rigid ? 0 : i]);
    // Signed-byte metadata survives float24 conversion exactly; no rounded weight test.
    if (rigid)
        dst.offsets[3] = -1;
#ifdef SRR2_OLD3DS_GPU_SKIN_PACKED
    if(!CanPack(dst)) return false;
#endif
    return true;
}
// The CPU composes one affine transform per bone instead of applying model-view
// to every GPU vertex. Positions and first-influence normals share this palette.
inline bool EyeViewValid(const float view[4][4])
{
    for(unsigned r = 0; r < 4; ++r)
        for(unsigned c = 0; c < 4; ++c)
            if(!std::isfinite(view[r][c])) return false;
    return view[0][3] == 0 && view[1][3] == 0 && view[2][3] == 0 && view[3][3] == 1;
}
inline bool PackEyeMatrixValidatedView(float out[3][4], const float bone[4][4], const float view[4][4])
{
    for(unsigned r = 0; r < 4; ++r)
        for(unsigned c = 0; c < 4; ++c)
            if(!std::isfinite(bone[r][c])) return false;
    for(unsigned r = 0; r < 3; ++r)
        for(unsigned c = 0; c < 4; ++c)
        {
            out[r][c] = (bone[c][0] * view[0][r] + bone[c][1] * view[1][r]) +
                         bone[c][2] * view[2][r];
            if(c == 3) out[r][c] += view[3][r];
            if(!std::isfinite(out[r][c])) return false;
        }
    return true;
}
inline bool PackEyeMatrix(float out[3][4], const float bone[4][4], const float view[4][4])
{
    return EyeViewValid(view) && PackEyeMatrixValidatedView(out, bone, view);
}
inline bool PackMatrix(float out[3][4], const float in[4][4])
{
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c)
            if (!std::isfinite(in[r][c]))
                return false;
    // CPU skinning uses only the affine components, regardless of the last column.
    for (unsigned r = 0; r < 3; ++r)
        for (unsigned c = 0; c < 4; ++c)
            out[r][c] = in[c][r];
    return true;
}
} // namespace picaSkin
#endif
