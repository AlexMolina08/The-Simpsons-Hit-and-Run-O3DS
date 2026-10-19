#pragma once

#include <stdint.h>

void BlockDecompressImageBC1(uint32_t width, uint32_t height, const uint8_t* blockStorage, unsigned char* image);
void BlockDecompressImageBC2(uint32_t width, uint32_t height, const uint8_t* blockStorage, unsigned char* image);
void BlockDecompressImageBC3(uint32_t width, uint32_t height, const uint8_t* blockStorage, unsigned char* image);

// Decode one 4x4 block to 16 tightly packed RGBA pixels. kind is BC1, BC2 or BC3.
void BlockDecompressBC(unsigned kind, const uint8_t* block, unsigned int* rgba);
