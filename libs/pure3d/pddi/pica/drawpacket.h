#ifndef PICA_DRAW_PACKET_H
#define PICA_DRAW_PACKET_H
#include <citro3d.h>
#ifdef __cplusplus
extern "C" {
#endif
void picaDrawPacketElements(GPU_Primitive_t primitive,int count,int type,const void* indices);
void picaDrawPacketArrays(GPU_Primitive_t primitive,int first,int count);
// Pure encoder: no SDK state changes and no command-buffer access.
unsigned picaEncodeDrawPacket(u32* output,GPU_Primitive_t primitive,int count,bool indexed,u32 offset,int type,int first);
#ifdef __cplusplus
}
#endif
#endif
