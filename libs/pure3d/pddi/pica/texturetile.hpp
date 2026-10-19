#ifndef PICA_TEXTURETILE_HPP
#define PICA_TEXTURETILE_HPP
#include <stdint.h>
namespace picaTile
{
enum Format { RGBA8, RGBA5551, LA8, RGBA4, RGB565 };
template<Format F> struct Pixel
{
    typedef unsigned short Type;
    static Type Convert(unsigned int p)
    {
        const unsigned r = p & 255, g = (p >> 8) & 255, b = (p >> 16) & 255, a = p >> 24;
        if(F == RGBA5551) return ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (a >> 7);
        if(F == LA8) return (((77 * r + 150 * g + 29 * b) >> 8) << 8) | a;
        if(F == RGBA4) return ((r >> 4) << 12) | ((g >> 4) << 8) | ((b >> 4) << 4) | (a >> 4);
        return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }
};
template<> struct Pixel<RGBA8>
{
    typedef unsigned int Type;
    static Type Convert(unsigned int p)
    {
        return (p << 24) | ((p & 0xff00) << 8) | ((p >> 8) & 0xff00) | (p >> 24);
    }
};
inline unsigned Morton(unsigned x, unsigned y)
{
    return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) |
           ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
}
template<Format F> void Convert(const unsigned int* rgba, unsigned w, unsigned h, void* data, void (*yield)())
{
    typename Pixel<F>::Type* dst = static_cast<typename Pixel<F>::Type*>(data);
    // The upload dimensions are PICA-valid multiples of eight. Work within one
    // 8x8 tile, keeping destination writes in a single 128/256-byte cache region.
    for(unsigned ty = 0; ty < h; ty += 8)
    {
        if(yield) yield();
        for(unsigned tx = 0; tx < w; tx += 8, dst += 64)
            for(unsigned y = 0; y < 8; ++y)
            {
                const unsigned int* row = rgba + (ty + y) * w + tx;
                for(unsigned x = 0; x < 8; ++x)
                    dst[Morton(x, y)] = Pixel<F>::Convert(row[x]);
            }
    }
}
template<Format F> void Block(const unsigned int* rgba, unsigned x, unsigned y, unsigned width, void* data)
{
    typename Pixel<F>::Type* dst = static_cast<typename Pixel<F>::Type*>(data);
    // A DXT block occupies exactly one quadrant of a PICA tile. Reverse rows
    // here, retaining the PC texture orientation without an image-sized flip.
    dst += ((y / 8) * (width / 8) + x / 8) * 64;
    const unsigned base = Morton(x & 7, y & 7);
    for(unsigned row = 0; row < 4; ++row)
        for(unsigned col = 0; col < 4; ++col)
            dst[base + Morton(col, row)] = Pixel<F>::Convert(rgba[(3 - row) * 4 + col]);
}
}
#endif
