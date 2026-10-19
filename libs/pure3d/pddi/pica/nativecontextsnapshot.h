#ifndef PICA_NATIVE_CONTEXT_SNAPSHOT_H
#define PICA_NATIVE_CONTEXT_SNAPSHOT_H
#include "citro3d_171/internal.h"
#include "nativeuniformsnapshot.h"
// Every dereferenced texture field and cube face address belongs to the record.
// data addresses are GPU inputs, never memory read by the command compiler.
typedef struct {
    C3D_Context context;
    C3D_Tex textures[3];
    C3D_TexCube cubes[3];
    unsigned textureMask;
    picaNativeUniformSnapshot uniforms[2];
} picaNativeContextSnapshot;
// Capture requires supported state and room for 2,048 output words. It consumes
// producer dirty state; callers must not fail or drop the captured record.
// Compile consumes its owned record and explicit cache, never live SDK globals.
// A shared cache requires a join before any legacy upload or invalidation.
bool picaNativeCanCaptureContext(void);
void picaNativeCaptureContext(picaNativeContextSnapshot* snapshot);
void picaNativeCompileContext(picaNativeCommandBlock* output,
    picaNativeContextSnapshot* snapshot,picaNativeUniformCache* cache);
#endif
