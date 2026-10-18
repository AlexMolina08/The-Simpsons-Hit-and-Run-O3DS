#ifndef PICA_PREPARED_GEOMETRY_HPP
#define PICA_PREPARED_GEOMETRY_HPP
#include <citro3d.h>
#ifdef SRR2_OLD3DS_DRAW_PACKET
#include <pddi/pica/drawpacket.h>
#endif
extern "C" void C3Di_UpdateContext(void);
// Owns selected draw metadata. Backing vertex/index allocations must remain alive
// and unchanged until the consumer fence, then through the existing GPU lifetime.
struct picaPreparedGeometry {
    const unsigned char* vertices;
    const unsigned short* indices;
    unsigned count,stride,attributes;
    u32 offset;
    u64 permutation;
    GPU_Primitive_t primitive;
    bool indexed,omitNormal,skin,wheelPalette;
};
inline void picaBindPreparedGeometry(const picaPreparedGeometry& input,bool& bufferInfoSet)
{
    if(!bufferInfoSet)
    {
        C3D_BufInfo info;
        BufInfo_Init(&info);
        BufInfo_Add(&info,input.vertices,input.stride,input.attributes,input.permutation);
        C3D_SetBufInfo(&info);
        C3Di_UpdateContext();
        bufferInfoSet=true;
    }
    GPUCMD_AddWrite(GPUREG_ATTRIBBUFFER0_OFFSET,input.offset);
}
inline void picaSubmitPreparedGeometry(const picaPreparedGeometry& input)
{
#ifdef SRR2_OLD3DS_DRAW_PACKET
    if(input.indexed)picaDrawPacketElements(input.primitive,(int)input.count,C3D_UNSIGNED_SHORT,input.indices);
    else picaDrawPacketArrays(input.primitive,0,(int)input.count);
#else
    if(input.indexed)C3D_DrawElements(input.primitive,(int)input.count,C3D_UNSIGNED_SHORT,input.indices);
    else C3D_DrawArrays(input.primitive,0,(int)input.count);
#endif
}
#endif
