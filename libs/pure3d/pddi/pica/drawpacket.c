/* Adapted from Citro3D 1.7.1 drawElements.c and drawArrays.c (fincs).
 * Alteration: emit identical command words in one bounded raw append.
 * Original license: citro3d_171/LICENSE. No GPU state, shader, ordering,
 * context synchronization or DrawUsed bookkeeping is omitted.
 */
#include "drawpacket.h"
#include "citro3d_171/internal.h"
#include <stddef.h>
#include <string.h>
#ifdef SRR2_OLD3DS_GPU_LISTS
#include "nativecontext.h"
#endif
_Static_assert(offsetof(C3D_Context,flags)==32,"Unsupported Citro3D flags ABI");
_Static_assert(offsetof(C3D_Context,bufInfo)+offsetof(C3D_BufInfo,base_paddr)==64,"Unsupported Citro3D buffer ABI");
#define CMD(reg,mask,value) (value),GPUCMD_HEADER(0,(mask),(reg))
static const u32 indexedTriangles[]={
 CMD(GPUREG_PRIMITIVE_CONFIG,2,GPU_GEOMETRY_PRIM),
 CMD(GPUREG_RESTART_PRIMITIVE,15,1),CMD(GPUREG_INDEXBUFFER_CONFIG,15,0),
 CMD(GPUREG_NUMVERTICES,15,0),CMD(GPUREG_VERTEX_OFFSET,15,0),
 CMD(GPUREG_GEOSTAGE_CONFIG,2,0x100),CMD(GPUREG_GEOSTAGE_CONFIG2,2,0x100),
 CMD(GPUREG_START_DRAW_FUNC0,1,0),CMD(GPUREG_DRAWELEMENTS,15,1),
 CMD(GPUREG_START_DRAW_FUNC0,1,1),
 CMD(GPUREG_GEOSTAGE_CONFIG,2,0),CMD(GPUREG_GEOSTAGE_CONFIG2,2,0),
 CMD(GPUREG_VTX_FUNC,15,1),CMD(GPUREG_PRIMITIVE_CONFIG,8,0),CMD(GPUREG_PRIMITIVE_CONFIG,8,0)
};
static const u32 indexedOther[]={
 CMD(GPUREG_PRIMITIVE_CONFIG,2,0),CMD(GPUREG_RESTART_PRIMITIVE,15,1),
 CMD(GPUREG_INDEXBUFFER_CONFIG,15,0),CMD(GPUREG_NUMVERTICES,15,0),
 CMD(GPUREG_VERTEX_OFFSET,15,0),CMD(GPUREG_START_DRAW_FUNC0,1,0),
 CMD(GPUREG_DRAWELEMENTS,15,1),CMD(GPUREG_START_DRAW_FUNC0,1,1),
 CMD(GPUREG_VTX_FUNC,15,1),CMD(GPUREG_PRIMITIVE_CONFIG,8,0),CMD(GPUREG_PRIMITIVE_CONFIG,8,0)
};
static const u32 arrays[]={
 CMD(GPUREG_PRIMITIVE_CONFIG,2,0),CMD(GPUREG_RESTART_PRIMITIVE,15,1),
 CMD(GPUREG_INDEXBUFFER_CONFIG,15,0x80000000),CMD(GPUREG_NUMVERTICES,15,0),
 CMD(GPUREG_VERTEX_OFFSET,15,0),CMD(GPUREG_GEOSTAGE_CONFIG2,1,1),
 CMD(GPUREG_START_DRAW_FUNC0,1,0),CMD(GPUREG_DRAWARRAYS,15,1),
 CMD(GPUREG_START_DRAW_FUNC0,1,1),CMD(GPUREG_GEOSTAGE_CONFIG2,1,0),CMD(GPUREG_VTX_FUNC,15,1)
};
unsigned picaEncodeDrawPacket(u32* packet,GPU_Primitive_t primitive,int count,bool indexed,u32 offset,int type,int first)
{
 const bool triangle=indexed && primitive==GPU_TRIANGLES;
 const size_t bytes=!indexed?sizeof(arrays):triangle?sizeof(indexedTriangles):sizeof(indexedOther);
 memcpy(packet,!indexed?arrays:triangle?indexedTriangles:indexedOther,bytes);
 if(!triangle)packet[0]=primitive;
 packet[6]=count;
 if(indexed)packet[4]=offset|(type<<31);else packet[8]=first;
 return bytes/sizeof(u32);
}
void picaDrawPacketElements(GPU_Primitive_t primitive,int count,int type,const void* indices)
{
 C3D_Context* ctx=C3Di_GetContext();
 const u32 pa=osConvertVirtToPhys(indices),base=ctx->bufInfo.base_paddr;
 if(pa<base) return;
#ifdef SRR2_OLD3DS_GPU_LISTS
 if(picaNativeRetainedDraw(primitive,count,true,pa-base,type,0))return;
#endif
 C3Di_UpdateContext();
 u32 packet[sizeof(indexedTriangles)/sizeof(u32)];
 const unsigned words=picaEncodeDrawPacket(packet,primitive,count,true,pa-base,type,0);
 GPUCMD_AddRawCommands(packet,words);
 C3Di_GetContext()->flags|=C3DiF_DrawUsed;
}
void picaDrawPacketArrays(GPU_Primitive_t primitive,int first,int count)
{
#ifdef SRR2_OLD3DS_GPU_LISTS
 if(picaNativeRetainedDraw(primitive,count,false,0,0,first))return;
#endif
 C3Di_UpdateContext();
 u32 packet[sizeof(arrays)/sizeof(u32)];
 const unsigned words=picaEncodeDrawPacket(packet,primitive,count,false,0,0,first);
 GPUCMD_AddRawCommands(packet,words);
 C3Di_GetContext()->flags|=C3DiF_DrawUsed;
}
