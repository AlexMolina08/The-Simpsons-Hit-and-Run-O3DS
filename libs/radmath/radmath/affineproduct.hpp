#ifndef RMT_AFFINE_PRODUCT_HPP
#define RMT_AFFINE_PRODUCT_HPP
#include <cstdint>
namespace rmtAffineProduct {
inline bool Overlaps(const void* left,const void* right)
{
    const auto a=reinterpret_cast<std::uintptr_t>(left),b=reinterpret_cast<std::uintptr_t>(right);
    return (a>=b ? a-b : b-a)<64;
}
inline bool Separate(const void* output,const void* a,const void* b)
{
    return !Overlaps(output,a) && !Overlaps(output,b);
}
// The caller proves output does not overlap either input. Inputs may overlap
// each other. Keep the original scalar products and their addition grouping;
// this permits load reuse without changing arithmetic or floating-point mode.
inline void Multiply(float (*__restrict output)[4],const float a[4][4],const float b[4][4])
{
    output[0][0] = (a[0][0] * b[0][0]) + (a[0][1] * b[1][0]) + (a[0][2] * b[2][0]);
    output[0][1] = (a[0][0] * b[0][1]) + (a[0][1] * b[1][1]) + (a[0][2] * b[2][1]);
    output[0][2] = (a[0][0] * b[0][2]) + (a[0][1] * b[1][2]) + (a[0][2] * b[2][2]);
    output[0][3] = 0.0f;

    output[1][0] = (a[1][0] * b[0][0]) + (a[1][1] * b[1][0]) + (a[1][2] * b[2][0]);
    output[1][1] = (a[1][0] * b[0][1]) + (a[1][1] * b[1][1]) + (a[1][2] * b[2][1]);
    output[1][2] = (a[1][0] * b[0][2]) + (a[1][1] * b[1][2]) + (a[1][2] * b[2][2]);
    output[1][3] = 0.0f;

    output[2][0] = (a[2][0] * b[0][0]) + (a[2][1] * b[1][0]) + (a[2][2] * b[2][0]);
    output[2][1] = (a[2][0] * b[0][1]) + (a[2][1] * b[1][1]) + (a[2][2] * b[2][1]);
    output[2][2] = (a[2][0] * b[0][2]) + (a[2][1] * b[1][2]) + (a[2][2] * b[2][2]);
    output[2][3] = 0.0f;

    output[3][0] = (a[3][0] * b[0][0]) + (a[3][1] * b[1][0]) + (a[3][2] * b[2][0]) + b[3][0];
    output[3][1] = (a[3][0] * b[0][1]) + (a[3][1] * b[1][1]) + (a[3][2] * b[2][1]) + b[3][1];
    output[3][2] = (a[3][0] * b[0][2]) + (a[3][1] * b[1][2]) + (a[3][2] * b[2][2]) + b[3][2];
    output[3][3] = 1.0f;
}
}
#endif
