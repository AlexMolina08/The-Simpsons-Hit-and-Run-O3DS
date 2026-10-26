#ifndef PICA_NATIVE_CONTEXT_H
#define PICA_NATIVE_CONTEXT_H
#include <citro3d.h>
#ifdef __cplusplus
extern "C" {
#endif
extern bool g_picaNativeContextEnabled;
void picaNativeRegisterPrograms(const shaderProgram_s* normal,const shaderProgram_s* skin);
void picaNativeRegisterReflectionProgram(const shaderProgram_s* reflection);
void picaNativeRegisterContactProgram(const shaderProgram_s* contact);
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
void picaNativeConfigureReflectionOutputs(unsigned ordinaryMap,unsigned ordinaryClock,unsigned reflectedMap,unsigned reflectedClock);
void picaNativeSetReflectionOutput(bool reflected);
void picaNativeInvalidateReflectionOutputs(void);
void picaNativeEnableSparseReflectionOutputs(bool enabled);
void picaNativeReflectionOutputStats(unsigned* ordinary,unsigned* reflected,unsigned* enabled);
void picaNativeConfigureReflectionEntries(const shaderProgram_s* mono,unsigned monoOrdinary,unsigned monoReflected,
                                         const shaderProgram_s* stereo,unsigned stereoOrdinary,unsigned stereoReflected);
void picaNativeReflectionEntryStats(unsigned* ordinary,unsigned* reflected,unsigned* enabled);
#endif
void picaNativeBindTexture1(C3D_Tex* texture);
bool picaNativeNormalProgramBound(void);
C3D_Tex* picaNativeTexture0(void);
void picaNativeTexture0Alias(C3D_Tex*);
void picaNativeUpdateContext(void);
void picaNativeContextStats(unsigned* fast,unsigned* fallback,unsigned* words);
#ifdef __cplusplus
}
#endif
#endif
