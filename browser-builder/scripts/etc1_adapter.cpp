// Browser entry point for tex3ds 2.3.0's ETC1 encoder.
// The tile order and byte order follow tex3ds source/encode.cpp.
// This adapter is GPL-3.0-or-later; upstream rg_etc1 remains zlib licensed.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "rg_etc1.h"

extern "C" int encodeEtc1(const unsigned char* rgb, unsigned width, unsigned height, unsigned char* output) {
    if (!rgb || !output || width < 8 || height < 8 || width > 1024 || height > 1024 || width % 8 || height % 8) return -1;
    static bool initialized = false;
    if (!initialized) { rg_etc1::pack_etc1_block_init(); initialized = true; }
    rg_etc1::etc1_pack_params params;
    params.clear();
    params.m_quality = rg_etc1::cHighQuality;
    unsigned offset = 0;
    for (unsigned tileY = 0; tileY < height; tileY += 8) {
        for (unsigned tileX = 0; tileX < width; tileX += 8) {
            for (unsigned blockY = 0; blockY < 8; blockY += 4) {
                for (unsigned blockX = 0; blockX < 8; blockX += 4) {
                    uint32_t pixels[16];
                    unsigned char encoded[8];
                    for (unsigned y = 0; y < 4; ++y) {
                        for (unsigned x = 0; x < 4; ++x) {
                            const unsigned char* source = rgb + ((tileY + blockY + y) * width + tileX + blockX + x) * 3;
                            pixels[y * 4 + x] = uint32_t(source[0]) | (uint32_t(source[1]) << 8) | (uint32_t(source[2]) << 16) | 0xff000000u;
                        }
                    }
                    rg_etc1::pack_etc1_block(encoded, pixels, params);
                    for (unsigned i = 0; i < 8; ++i) output[offset++] = encoded[7 - i];
                }
            }
        }
    }
    return static_cast<int>(offset);
}

#ifdef ETC1_TEST_MAIN
int main(int argc, char** argv) {
    if (argc != 5) return 2;
    unsigned width = std::strtoul(argv[1], nullptr, 10);
    unsigned height = std::strtoul(argv[2], nullptr, 10);
    std::vector<unsigned char> input(width * height * 3), output(width * height / 2);
    FILE* source = std::fopen(argv[3], "rb");
    if (!source || std::fread(input.data(), 1, input.size(), source) != input.size()) return 3;
    std::fclose(source);
    int bytes = encodeEtc1(input.data(), width, height, output.data());
    if (bytes < 0) return 4;
    FILE* destination = std::fopen(argv[4], "wb");
    if (!destination || std::fwrite(output.data(), 1, output.size(), destination) != output.size()) return 5;
    return std::fclose(destination);
}
#endif
