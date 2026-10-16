#ifndef PDDI_PICA_SWIZZLE_HPP
#define PDDI_PICA_SWIZZLE_HPP

#include <stddef.h>

void picaSwizzle(void* dst, const void* src, int w, int h, int bpp);

void picaSwizzleYUV420toRGB565(void* dst, const unsigned char* yPlane,
    const unsigned char* crPlane, const unsigned char* cbPlane, int width, int height,
    int textureWidth, int yPitch, int chromaPitch);

unsigned picaSwizzleOffset(int x, int y, int w);
#endif
