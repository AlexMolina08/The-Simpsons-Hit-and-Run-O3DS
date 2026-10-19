#ifndef PICA_TRACKED_UNIFORMS_HPP
#define PICA_TRACKED_UNIFORMS_HPP
#include <citro3d.h>
extern "C" unsigned g_picaNativeUniformMask[2][3];
// This backend owns all float-uniform writes to its normal/skin programs.
// Shader constants and restore are tracked by the private uniform manager.
static inline void picaTrackUniforms(GPU_SHADER_TYPE type,int first,int count)
{
    while(count>0) {
        const unsigned block=unsigned(first)>>5,shift=unsigned(first)&31;
        const unsigned n=unsigned(count)<32-shift?unsigned(count):32-shift;
        g_picaNativeUniformMask[type][block]|=(~0u>>(32-n))<<shift;
        first+=n;count-=n;
    }
}
static inline C3D_FVec* picaTrackedWritePtr(GPU_SHADER_TYPE type,int id,int count)
{picaTrackUniforms(type,id,count);return C3D_FVUnifWritePtr(type,id,count);}
static inline void picaTrackedSet(GPU_SHADER_TYPE type,int id,float x,float y,float z,float w)
{picaTrackUniforms(type,id,1);C3D_FVUnifSet(type,id,x,y,z,w);}
static inline void picaTrackedMtx4(GPU_SHADER_TYPE type,int id,const C3D_Mtx* matrix)
{picaTrackUniforms(type,id,4);C3D_FVUnifMtx4x4(type,id,matrix);}
#define C3D_FVUnifWritePtr picaTrackedWritePtr
#define C3D_FVUnifSet picaTrackedSet
#define C3D_FVUnifMtx4x4 picaTrackedMtx4
#endif
