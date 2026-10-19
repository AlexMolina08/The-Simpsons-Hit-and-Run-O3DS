#ifndef PICA_NATIVE_UNIFORM_SNAPSHOT_H
#define PICA_NATIVE_UNIFORM_SNAPSHOT_H
#include <citro3d.h>
// Producer owns dirty SDK arrays. Compiler owns only this upload history.
typedef struct {
    C3D_FVec values[2][C3D_FVUNIF_COUNT];
    bool valid[2][C3D_FVUNIF_COUNT],fast[2];
    unsigned avoided,sent;
} picaNativeUniformCache;
#ifdef SRR2_OLD3DS_NATIVE_CONTEXT
#include "nativecommands.h"
typedef struct {
    unsigned count,constantCount,intMask;
    bool boolDirty;
    u16 boolValue;
    unsigned char ids[C3D_FVUNIF_COUNT];
    C3D_FVec values[C3D_FVUNIF_COUNT];
    float24Uniform_s constants[8];
    C3D_IVec integers[C3D_IVUNIF_COUNT];
} picaNativeUniformSnapshot;
typedef struct {
    bool floatDirty[2][C3D_FVUNIF_COUNT],floatEver[2][C3D_FVUNIF_COUNT];
    bool intDirty[2][C3D_IVUNIF_COUNT],intEver[2][C3D_IVUNIF_COUNT],boolDirty[2],constantDirty[2];
    unsigned masks[2][3];
} picaNativeUniformProducerState;
#ifdef SRR2_OLD3DS_PREPARED_THREAD
typedef struct {
    picaNativeUniformProducerState producer;
    picaNativeUniformCache cache;
    C3D_FVec floats[2][C3D_FVUNIF_COUNT];
    C3D_IVec integers[2][C3D_IVUNIF_COUNT];
    u16 booleans[2];
    int constantCount[2];float24Uniform_s* constantData[2];
} picaNativeUniformCheckpoint;
void picaNativeSaveUniformCheckpoint(picaNativeUniformCheckpoint*);
void picaNativeRestoreUniformCheckpoint(const picaNativeUniformCheckpoint*);
#endif
void picaNativeSaveUniformProducer(picaNativeUniformProducerState* state);
void picaNativeRestoreUniformProducer(const picaNativeUniformProducerState* state);
bool picaNativeSameUniformProducer(const picaNativeUniformProducerState* a,const picaNativeUniformProducerState* b);
void picaNativeCaptureUniforms(GPU_SHADER_TYPE type,picaNativeUniformSnapshot* snapshot);
void picaNativeCompileUniforms(picaNativeCommandBlock* output,GPU_SHADER_TYPE type,
    const picaNativeUniformSnapshot* snapshot,picaNativeUniformCache* cache);
picaNativeUniformCache* picaNativeSharedUniformCache(void);
#endif
#endif
