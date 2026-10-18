#ifndef PICA_PREPARED_SKIN_HPP
#define PICA_PREPARED_SKIN_HPP
#include <citro3d.h>
#include <pddi/pica/skincore.hpp>
#ifdef SRR2_OLD3DS_NATIVE_UNIFORMS
#include <pddi/pica/trackeduniforms.hpp>
#endif

// Packed directly into owned storage; there is no borrowed p3d palette/scratch
// pointer and no second full-palette copy. Retain this record until consumption.
struct picaPreparedSkinPalette {
    float values[picaSkin::MaxBones][3][4];
    unsigned count;
    float (*operator[](unsigned index))[4]{return values[index];}
};
inline bool picaUploadPreparedSkinPalette(const picaPreparedSkinPalette& input,unsigned first)
{
    if(!input.count || input.count>picaSkin::MaxBones || first>96 || 3*input.count>96-first)return false;
    C3D_FVec* destination=C3D_FVUnifWritePtr(GPU_VERTEX_SHADER,first,3*input.count);
    for(unsigned bone=0;bone<input.count;++bone)
        for(unsigned row=0;row<3;++row)
        {
            destination[3*bone+row].x=input.values[bone][row][0];
            destination[3*bone+row].y=input.values[bone][row][1];
            destination[3*bone+row].z=input.values[bone][row][2];
            destination[3*bone+row].w=input.values[bone][row][3];
        }
    return true;
}
#endif
