#ifndef PICA_NATIVE_CONTEXT_H
#define PICA_NATIVE_CONTEXT_H
#include <citro3d.h>
#ifdef __cplusplus
extern "C" {
#endif
extern bool g_picaNativeContextEnabled;
void picaNativeRegisterPrograms(const shaderProgram_s* normal,const shaderProgram_s* skin);
bool picaNativeNormalProgramBound(void);
C3D_Tex* picaNativeTexture0(void);
void picaNativeTexture0Alias(C3D_Tex*);
void picaNativeUpdateContext(void);
#ifdef SRR2_OLD3DS_GPU_LISTS
bool picaNativeRetainedDraw(GPU_Primitive_t primitive,int count,bool indexed,u32 offset,int type,int first);
#endif
#ifdef SRR2_OLD3DS_PREPARED_THREAD
void* picaNativeAllocateCheckpoint(void);
void picaNativeFreeCheckpoint(void*);
void picaNativeSaveCheckpoint(void*);
void picaNativeRestoreCheckpoint(const void*);
bool picaNativeCompareCheckpoint(const void*,const void*);
#endif
void picaNativeContextStats(unsigned* fast,unsigned* fallback,unsigned* words);
#ifdef __cplusplus
}
#endif
#endif
