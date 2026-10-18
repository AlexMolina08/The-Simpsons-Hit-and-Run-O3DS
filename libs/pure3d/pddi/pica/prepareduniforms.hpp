#ifndef PICA_PREPARED_UNIFORMS_HPP
#define PICA_PREPARED_UNIFORMS_HPP
#include <citro3d.h>
#include <pddi/pddi.hpp>
#include <pddi/base/basecontext.hpp>
#include <pddi/pica/unlitprojection.hpp>
#include <cstring>
#ifdef SRR2_OLD3DS_NATIVE_UNIFORMS
#include <pddi/pica/trackeduniforms.hpp>
#endif
#include "pddi_shbin.h"
#ifdef SRR2_OLD3DS_GPU_SKIN
#include "skin_shbin.h"
#endif
// Renderer inputs, not a second copy of already prepared SDK uniform arrays.
// Every matrix, colour and light value belongs to this record. No scratch pointer
// or material/context pointer survives capture. SDK writes require one owner.
struct picaPreparedUniformInput {
    pddiMatrix modelView;
    C3D_Mtx projection;
    pddiColour ambient,diffuse,emissive,sceneAmbient;
    C3D_FVec lightPosition[PDDI_MAX_LIGHTS],lightColour[PDDI_MAX_LIGHTS],lightAttenuation[PDDI_MAX_LIGHTS];
    C3D_FVec directionalPosition[PDDI_MAX_LIGHTS];
    float colours[3][4];
    unsigned invalidationEpoch,lightEpoch,projectionEpoch;
    int lightCount;
    bool lit,skin,stereo,stereoActive,directional,hasPreparedColours;
};
struct picaPreparedViewUniforms {C3D_Mtx projection;bool stereo,stereoActive;unsigned epoch;};
struct picaPreparedLightUniforms {
    C3D_FVec position[PDDI_MAX_LIGHTS],colour[PDDI_MAX_LIGHTS],attenuation[PDDI_MAX_LIGHTS],directionalPosition[PDDI_MAX_LIGHTS];
    pddiColour sceneAmbient;unsigned epoch;int count;bool directional;
};
struct picaPreparedMaterialUniforms {
    pddiColour ambient,diffuse,emissive;float colours[3][4];bool lit,hasPreparedColours;
};
struct picaPreparedTransformUniforms {pddiMatrix modelView;unsigned invalidationEpoch;bool skin;};
// Production draw records can share immutable view/light/material records until
// their batch fence. This view avoids reconstructing a full SDK/input snapshot
// per draw. References must point to retained records, never producer scratch.
struct picaPreparedUniformView {
    const pddiMatrix& modelView;
    const C3D_Mtx& projection;
    const pddiColour& ambient;const pddiColour& diffuse;const pddiColour& emissive;const pddiColour& sceneAmbient;
    const C3D_FVec* lightPosition;const C3D_FVec* lightColour;const C3D_FVec* lightAttenuation;const C3D_FVec* directionalPosition;
    const float (*colours)[4];
    unsigned invalidationEpoch,lightEpoch,projectionEpoch;
    int lightCount;
    bool lit,skin,stereo,stereoActive,directional,hasPreparedColours;
    picaPreparedUniformView(const picaPreparedViewUniforms& view,const picaPreparedLightUniforms& lights,
        const picaPreparedMaterialUniforms& material,const picaPreparedTransformUniforms& transform):
        modelView(transform.modelView),projection(view.projection),ambient(material.ambient),diffuse(material.diffuse),emissive(material.emissive),sceneAmbient(lights.sceneAmbient),
        lightPosition(lights.position),lightColour(lights.colour),lightAttenuation(lights.attenuation),directionalPosition(lights.directionalPosition),colours(material.colours),
        invalidationEpoch(transform.invalidationEpoch),lightEpoch(lights.epoch),projectionEpoch(view.epoch),lightCount(lights.count),lit(material.lit),skin(transform.skin),stereo(view.stereo),stereoActive(view.stereoActive),directional(lights.directional),hasPreparedColours(material.hasPreparedColours){}
    explicit picaPreparedUniformView(const picaPreparedUniformInput& source):
        modelView(source.modelView),projection(source.projection),ambient(source.ambient),diffuse(source.diffuse),emissive(source.emissive),sceneAmbient(source.sceneAmbient),
        lightPosition(source.lightPosition),lightColour(source.lightColour),lightAttenuation(source.lightAttenuation),directionalPosition(source.directionalPosition),colours(source.colours),
        invalidationEpoch(source.invalidationEpoch),lightEpoch(source.lightEpoch),projectionEpoch(source.projectionEpoch),lightCount(source.lightCount),lit(source.lit),skin(source.skin),stereo(source.stereo),stereoActive(source.stereoActive),directional(source.directional),hasPreparedColours(source.hasPreparedColours){}
};
struct picaPreparedUniformCache {
    bool modelViewUploaded=false,projUploaded=false,lightsUploaded=false,matUploaded=false;
    bool lastLit=false,miscUploaded=false,sparseUnlitProjection=false,lastFastUnlit=false;
    pddiMatrix lastModelView;
    pddiColour lastMatAmbient,lastMatDiffuse,lastMatEmissive,lastSceneAmbient;
    unsigned invalidationEpoch=~0u,lightEpoch=~0u,projectionEpoch=~0u;
    unsigned modelViewUploads=0,modelViewReuses=0,fastUnlitCalls=0,otherProjectionCalls=0,directionalDraws=0,generalLightDraws=0;
    void Invalidate(){modelViewUploaded=projUploaded=lightsUploaded=matUploaded=miscUploaded=false;}
};
// Synchronous integration aliases the live legacy cache rather than copying it.
// The asynchronous consumer will own a picaPreparedUniformCache instead.
struct picaPreparedUniformCacheRefs {
    bool &modelViewUploaded,&projUploaded,&lightsUploaded,&matUploaded;
    bool &lastLit,&miscUploaded,&sparseUnlitProjection,&lastFastUnlit;
    pddiMatrix& lastModelView;
    pddiColour &lastMatAmbient,&lastMatDiffuse,&lastMatEmissive,&lastSceneAmbient;
    unsigned invalidationEpoch,lightEpoch,projectionEpoch;
    unsigned &modelViewUploads,&modelViewReuses,&fastUnlitCalls,&otherProjectionCalls,&directionalDraws,&generalLightDraws;
    void Invalidate(){modelViewUploaded=projUploaded=lightsUploaded=matUploaded=miscUploaded=false;}
};
template<class UniformInput,class UniformCache>
inline void picaApplyPreparedUniforms(const UniformInput& input,UniformCache& cache)
{
    if(cache.invalidationEpoch!=input.invalidationEpoch){cache.Invalidate();cache.invalidationEpoch=input.invalidationEpoch;}
    if(cache.lightEpoch!=input.lightEpoch){cache.lightsUploaded=false;cache.lightEpoch=input.lightEpoch;}
    if(cache.projectionEpoch!=input.projectionEpoch){cache.projUploaded=false;cache.projectionEpoch=input.projectionEpoch;}
    const bool lit=input.lit,skinProgramBound=input.skin,stereoOn=input.stereo,directionalOnly=input.directional;
    const int nActiveLights=input.lightCount;
    const pddiColour ambient=input.ambient,diffuse=input.diffuse,emissive=input.emissive;
    const C3D_Mtx& hwProjection=input.projection;
    const C3D_FVec* lightPos=input.lightPosition;
    const C3D_FVec* lightColour=input.lightColour;
    const C3D_FVec* lightAtten=input.lightAttenuation;
    const C3D_FVec* directionalPos=input.directionalPosition;
    const float (*prepared)[4]=input.hasPreparedColours?input.colours:nullptr;
    (void)skinProgramBound;(void)stereoOn;(void)directionalOnly;(void)directionalPos;
#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
    const unsigned skinRegisterOffset = skinProgramBound ? 4 : 0;
#else
    const unsigned skinRegisterOffset = 0;
#endif
    const unsigned skin_lightPos = VSH_FVEC_u_lightPos - skinRegisterOffset;
    const unsigned skin_lightColour = VSH_FVEC_u_lightColour - skinRegisterOffset;
    const unsigned skin_lightAtten = VSH_FVEC_u_lightAtten - skinRegisterOffset;
    const unsigned skin_matAmbient = VSH_FVEC_u_matAmbient - skinRegisterOffset;
    const unsigned skin_matDiffuse = VSH_FVEC_u_matDiffuse - skinRegisterOffset;
    const unsigned skin_matEmissive = VSH_FVEC_u_matEmissive - skinRegisterOffset;
    const unsigned skin_sceneAmbient = VSH_FVEC_u_sceneAmbient - skinRegisterOffset;

#ifdef SRR2_OLD3DS_GPU_SKIN_PACKED
    // Upload alongside the other draw uniforms, after shader/material binding.
    // Do not depend on a preceding program switch having emitted this register.
    if(skinProgramBound)
    {
        static_assert(VSH_FVEC_u_eye_quant == 95, "Packed scales follow the bone palette");
        C3D_FVUnifSet(GPU_VERTEX_SHADER, VSH_FVEC_u_eye_quant,
                      1.0f / 8192.0f, 1.0f / 32768.0f, 1.0f / 4096.0f, 1.0f);
    }
#endif

    const pddiMatrix* mv = &input.modelView;

    if(!cache.projUploaded)
    {
#ifdef SRR2_OLD3DS_GPU_SKIN_COMPACT
        if(!skinProgramBound)
#endif
        C3D_FVUnifSet(GPU_VERTEX_SHADER, VSH_FVEC_u_stereoOn,
                      stereoOn ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, VSH_FVEC_u_proj, &hwProjection);
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
        if(!skinProgramBound)
#endif
        {
            float projection[4][4], scale[4];
            for(unsigned row=0;row<4;++row) {
                projection[row][0]=hwProjection.r[row].x;projection[row][1]=hwProjection.r[row].y;
                projection[row][2]=hwProjection.r[row].z;projection[row][3]=hwProjection.r[row].w;
            }
            cache.sparseUnlitProjection = picaUnlitProjection::Parts(projection, scale);
            if(cache.sparseUnlitProjection)
                C3D_FVUnifSet(GPU_VERTEX_SHADER, VSH_FVEC_u_unlitScale, scale[0],scale[1],scale[2],scale[3]);
        }
#endif
        cache.projUploaded = true;
    }

#ifdef SRR2_OLD_3DS
    // Material changes often draw the same transform repeatedly. Preserve exact
    // matrix bits and invalidate at every frame and render-target transition.
    if(!cache.modelViewUploaded || memcmp(&cache.lastModelView, mv, sizeof(cache.lastModelView)) != 0)
    {
        memcpy(&cache.lastModelView, mv, sizeof(cache.lastModelView));
        cache.modelViewUploaded = true;
        ++cache.modelViewUploads;
#endif
    C3D_Mtx hwModelView;
    for(int row = 0; row < 4; row++)
    {
        hwModelView.r[row].x = mv->Row4(0)[row];
        hwModelView.r[row].y = mv->Row4(1)[row];
        hwModelView.r[row].z = mv->Row4(2)[row];
        hwModelView.r[row].w = mv->Row4(3)[row];
    }

#ifdef SRR2_OLD3DS_GPU_SKIN_EYE_PALETTE
    if(!skinProgramBound)
#endif
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, VSH_FVEC_u_mv, &hwModelView);
#ifdef SRR2_OLD_3DS
    }
    else {
        ++cache.modelViewReuses; }
#endif

#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    // Only mono, unlit affine draws use the reduced transform. Lit, stereo and
    // custom projection paths retain the original instructions and uniforms.
    const bool fastUnlit =
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
        !skinProgramBound &&
#endif
        !lit && (!stereoOn || !input.stereoActive) && cache.sparseUnlitProjection &&
        mv->Row4(0)[3] == 0.0f && mv->Row4(1)[3] == 0.0f &&
        mv->Row4(2)[3] == 0.0f && mv->Row4(3)[3] == 1.0f;
#endif
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
    if(fastUnlit) ++cache.fastUnlitCalls; else ++cache.otherProjectionCalls;
#endif
    if(!cache.miscUploaded || lit != cache.lastLit
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
       || fastUnlit != cache.lastFastUnlit
#endif
      )
    {
#ifdef SRR2_OLD3DS_GPU_SKIN_COMPACT
        if(skinProgramBound)
        {
            // b1 replaces the lighting float; c24..c95 belong solely to bones.
            C3D_BoolUnifSet(GPU_VERTEX_SHADER, 0x69, lit);
            if(!lit)
            {
                // Unlit draws still need zero, one and the byte-color scale.
                C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matAmbient, 0, 0, 0, 1);
                C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matEmissive, 0, 0, 0, 0);
                C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_sceneAmbient, 0, 0, 0, 1.0f / 255.0f);
                cache.matUploaded = false;
            }
        }
        else
#endif
        C3D_FVUnifSet(GPU_VERTEX_SHADER, VSH_FVEC_u_misc,
                      lit ? 1.0f : 0.0f,
                      0.0f,
                      1.0f,
                      1.0f / 255.0f);
#ifdef SRR2_OLD3DS_UNLIT_PROJECTION
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
        if(!skinProgramBound)
#endif
        {
            static_assert(VSH_FLAG_u_fastUnlit == 2, "Fast unlit uses normal-program boolean b1");
            C3D_BoolUnifSet(GPU_VERTEX_SHADER, 0x69, fastUnlit);
        }
        cache.lastFastUnlit = fastUnlit;
#endif
        cache.lastLit = lit;
        cache.miscUploaded = true;
    }

    if(!lit)
        return;


#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
    if(lit) {if(directionalOnly) ++cache.directionalDraws;else ++cache.generalLightDraws;}
#endif
    if(!cache.lightsUploaded)
    {
#ifdef SRR2_OLD3DS_LIGHT_LOOP
        const int lightCount = nActiveLights;
#else
        const int lightCount = PDDI_MAX_LIGHTS;
#endif
        C3D_FVec* pos = C3D_FVUnifWritePtr(GPU_VERTEX_SHADER, skin_lightPos, lightCount);
        C3D_FVec* col = C3D_FVUnifWritePtr(GPU_VERTEX_SHADER, skin_lightColour, lightCount);
        C3D_FVec* att = C3D_FVUnifWritePtr(GPU_VERTEX_SHADER, skin_lightAtten, lightCount);
        for(int i = 0; i < lightCount; i++)
        {
#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
            pos[i] = directionalOnly ? directionalPos[i] : lightPos[i];
#else
            pos[i] = lightPos[i];
#endif
            col[i] = lightColour[i];
            att[i] = lightAtten[i];
        }
#ifdef SRR2_OLD3DS_LIGHT_LOOP
        // PICA loop counts encode iterations minus one. Zero lights use the
        // boolean guard rather than wrapping the unsigned count to 255.
        static_assert(VSH_FLAG_u_hasLights == 1, "light boolean register");
        C3D_IVUnifSet(GPU_VERTEX_SHADER, 0x60 + VSH_IVEC_u_lightLoop,
                      nActiveLights ? nActiveLights - 1 : 0, 0, 1, 0);
        C3D_BoolUnifSet(GPU_VERTEX_SHADER, 0x68, nActiveLights != 0);
#endif
#ifdef SRR2_OLD3DS_DIRECTIONAL_LIGHT
        static_assert(VSH_FLAG_u_directionalOnly == 4,"directional light boolean register");
        C3D_BoolUnifSet(GPU_VERTEX_SHADER,0x6a,directionalOnly);
#endif
        cache.lightsUploaded = true;
    }

    if(cache.matUploaded &&
       ambient.c  == cache.lastMatAmbient.c  &&
       diffuse.c  == cache.lastMatDiffuse.c  &&
       emissive.c == cache.lastMatEmissive.c &&
       input.sceneAmbient.c == cache.lastSceneAmbient.c)
    {
        return;
    }
    cache.lastMatAmbient   = ambient;
    cache.lastMatDiffuse   = diffuse;
    cache.lastMatEmissive  = emissive;
    cache.lastSceneAmbient = input.sceneAmbient;
    cache.matUploaded      = true;

    if(prepared)
    {
        C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matAmbient,
                      prepared[0][0], prepared[0][1], prepared[0][2], prepared[0][3]);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matDiffuse,
                      prepared[1][0], prepared[1][1], prepared[1][2], prepared[1][3]);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matEmissive,
                      prepared[2][0], prepared[2][1], prepared[2][2], prepared[2][3]);
    }
    else
    {
    C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matAmbient,
                  (float)ambient.Red() / 255.0f,
                  (float)ambient.Green() / 255.0f,
                  (float)ambient.Blue() / 255.0f,
                  1.0f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matDiffuse,
                  (float)diffuse.Red() / 255.0f,
                  (float)diffuse.Green() / 255.0f,
                  (float)diffuse.Blue() / 255.0f,
                  (float)diffuse.Alpha() / 255.0f);
    C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_matEmissive,
                  (float)emissive.Red() / 255.0f,
                  (float)emissive.Green() / 255.0f,
                  (float)emissive.Blue() / 255.0f,
                  0.0f);
    }
    C3D_FVUnifSet(GPU_VERTEX_SHADER, skin_sceneAmbient,
                  (float)input.sceneAmbient.Red() / 255.0f,
                  (float)input.sceneAmbient.Green() / 255.0f,
                  (float)input.sceneAmbient.Blue() / 255.0f,
#ifdef SRR2_OLD3DS_GPU_SKIN_COMPACT
                  skinProgramBound ? 1.0f / 255.0f : 1.0f);
#else
                  1.0f);
#endif
}
#endif
