#ifndef RAD_CTR_SOFTWARE_DECODER_HPP
#define RAD_CTR_SOFTWARE_DECODER_HPP

#include <stdint.h>
#include <stdio.h>

struct SoftwareMovieDecoder;
struct SoftwareMovieFrame
{
    const unsigned char* y;
    const unsigned char* cr;
    const unsigned char* cb;
    unsigned width, height, yPitch, chromaPitch;
};

SoftwareMovieDecoder* CreateSoftwareMovieDecoder(unsigned width, unsigned height);
void DestroySoftwareMovieDecoder(SoftwareMovieDecoder* decoder);
bool DecodeSoftwareMovieFrame(SoftwareMovieDecoder* decoder, FILE* file,
                             uint32_t end, uint32_t* position, SoftwareMovieFrame* output);

#endif
