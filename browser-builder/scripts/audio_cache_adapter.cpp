// Browser adapter for the production three-predictor DSP encoder. The build
// script extracts audio_cache_kernel.inc from the current ndsphal.cpp source;
// no clip, PCM sample or encoded game audio is included in this tool.
#include <cstdint>
#include <cstring>
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u8 = std::uint8_t;
#include "audio_cache_kernel.inc"

extern "C" unsigned encode_audio_cache(void* pcm, unsigned samples)
{
    if(!pcm || (reinterpret_cast<std::uintptr_t>(pcm) & 1u) ||
       samples < 4096u || samples > 441000u)
        return 0;
    return AdpcmEncodeInPlaceKernel<3>(pcm, samples);
}
