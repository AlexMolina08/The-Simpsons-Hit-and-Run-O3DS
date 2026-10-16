// Host regression: validate converted containers and decode every video frame.
#include "libs/radmovie/src/ctr/ctrsoftwaredecoder.hpp"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

struct Header
{
    char magic[8];
    uint32_t version;
    uint16_t width, height;
    uint32_t fpsNum, fpsDen, frames, rate;
    uint16_t channels, reserved;
    uint32_t audioOffset, audioBytes, videoOffset, videoBytes;
};
static_assert(sizeof(Header) == 52, "Movie header layout changed");

int main(int argc, char** argv)
{
    if(argc < 2) return 2;
    FILE* input = fopen(argv[1], "rb");
    Header header;
    if(!input || fread(&header, sizeof(header), 1, input) != 1) return 3;
    fseek(input, 0, SEEK_END);
    const long bytes = ftell(input);
    if(memcmp(header.magic, "SRR3DSMV", 8) || header.version != 2 ||
       header.width != 320 || header.height != 240 ||
       header.fpsNum != 30000 || header.fpsDen != 1001 ||
       header.audioOffset != sizeof(header) ||
       header.audioBytes % 4 ||
       header.videoOffset != header.audioOffset + header.audioBytes ||
       (uint64_t)header.videoOffset + header.videoBytes != (uint64_t)bytes) return 4;
    fseek(input, header.videoOffset, SEEK_SET);
    uint32_t position = header.videoOffset;
    SoftwareMovieDecoder* decoder = CreateSoftwareMovieDecoder(header.width, header.height);
    if(!decoder) return 10;
    std::vector<uint8_t> pixels(header.width * header.height * 3);
    SoftwareMovieFrame frame;
    uint32_t count = 0;
    uint32_t firstHash = 2166136261u, lastHash = 0;
    while(DecodeSoftwareMovieFrame(decoder, input, header.videoOffset + header.videoBytes, &position, &frame))
    {
        if(frame.width != header.width || frame.height != header.height) return 6;
        for(unsigned y=0; y<header.height; ++y)
            for(unsigned x=0; x<header.width; ++x)
            {
                const int yy = ((frame.y[y * frame.yPitch + x] - 16) * 76309) >> 16;
                const int cb = frame.cb[(y / 2) * frame.chromaPitch + x / 2] - 128;
                const int cr = frame.cr[(y / 2) * frame.chromaPitch + x / 2] - 128;
                const int rgb[3] = { yy + ((cr * 104597) >> 16),
                    yy - ((cb * 25674 + cr * 53278) >> 16), yy + ((cb * 132201) >> 16) };
                for(int c=0;c<3;++c) pixels[(y * header.width + x) * 3 + c] =
                    rgb[c] < 0 ? 0 : rgb[c] > 255 ? 255 : rgb[c];
            }
        uint32_t hash = 2166136261u;
        for(uint8_t pixel : pixels) hash = (hash ^ pixel) * 16777619u;
        if(count == 0)
        {
            firstHash = hash;
            if(argc > 2)
            {
                FILE* image = fopen(argv[2], "wb");
                if(!image) return 7;
                fprintf(image, "P6\n%u %u\n255\n", header.width, header.height);
                fwrite(pixels.data(), 1, pixels.size(), image);
                fclose(image);
            }
        }
        lastHash = hash;
        ++count;
        // Preserve decoder/parser lookahead while replacing the file handle,
        // matching the native suspend/resume path.
        if(count % 127 == 0)
        {
            fclose(input);
            input = fopen(argv[1], "rb");
            if(!input || fseek(input, position, SEEK_SET) != 0) return 11;
        }
    }
    DestroySoftwareMovieDecoder(decoder);
    fclose(input);
    printf("frames=%u expected=%u first_hash=%08x last_hash=%08x audio_bytes=%u\n",
        count, header.frames, firstHash, lastHash, header.audioBytes);
    return count == header.frames ? 0 : 8;
}
