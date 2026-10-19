// Compare fused uploads with scalar BT.601 conversion and existing tiling.
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "libs/pure3d/pddi/pica/swizzle.hpp"
#include <assert.h>
#include <vector>

int main()
{
    for(int width : {8, 16, 256, 320})
        for(int phase = 0; phase < 8; ++phase)
        {
            const int height = 240, textureWidth = 512;
            const int yPitch = width + 16, chromaPitch = width / 2 + 8;
            std::vector<unsigned char> yPlane(yPitch * height);
            std::vector<unsigned char> cbPlane(chromaPitch * height / 2);
            std::vector<unsigned char> crPlane(cbPlane.size());
            for(size_t i=0; i<yPlane.size(); ++i) yPlane[i] = (i * 13 + phase * 31) & 255;
            for(size_t i=0; i<cbPlane.size(); ++i)
            {
                cbPlane[i] = (i * 17 + phase * 73) & 255;
                crPlane[i] = (i * 19 + phase * 127) & 255;
            }
            std::vector<uint16_t> output(textureWidth * 256 + 1, 0xa55a);
            std::vector<uint16_t> reference(output);
            for(int y=0; y<height; ++y)
                for(int x=0; x<width; ++x)
                {
                    const int yy = ((int(yPlane[y * yPitch + x]) - 16) * 76309) >> 16;
                    const int cb = int(cbPlane[(y / 2) * chromaPitch + x / 2]) - 128;
                    const int cr = int(crPlane[(y / 2) * chromaPitch + x / 2]) - 128;
                    int rgb[3] = {yy + ((cr * 104597) >> 16),
                        yy - ((cb * 25674 + cr * 53278) >> 16), yy + ((cb * 132201) >> 16)};
                    for(int& channel : rgb) channel = channel < 0 ? 0 : channel > 255 ? 255 : channel;
                    reference[picaSwizzleOffset(x, y, textureWidth)] =
                        ((rgb[0] >> 3) << 11) | ((rgb[1] >> 2) << 5) | (rgb[2] >> 3);
                }
            picaSwizzleYUV420toRGB565(output.data(), yPlane.data(), crPlane.data(), cbPlane.data(),
                width, height, textureWidth, yPitch, chromaPitch);
            assert(output == reference);
        }
    puts("Movie upload: exact YUV/RGB565 colors, Morton layout, plane strides and padding passed");
}
