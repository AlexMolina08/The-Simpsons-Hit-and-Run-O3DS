#ifndef PICA_SKINCORE_HPP
#define PICA_SKINCORE_HPP
#include <cmath>
#include <cstdint>
#include <cstring>
namespace picaSkin
{
const unsigned MaxBones = 23;
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
    return true;
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
