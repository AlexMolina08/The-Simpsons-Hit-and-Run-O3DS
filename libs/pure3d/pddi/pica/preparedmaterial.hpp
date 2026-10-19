#ifndef PICA_PREPARED_MATERIAL_HPP
#define PICA_PREPARED_MATERIAL_HPP
#include <citro3d.h>
#include <pddi/pica/statecache.hpp>
#include <cstring>

// Captured material pass input. Texture storage must remain alive until the
// batch fence. Consumer applies sampler values to its copy, never to the asset.
struct picaPreparedMaterialPass {
    C3D_Tex texture;
    const C3D_Tex* textureIdentity;
    GPU_TEXTURE_FILTER_PARAM filter;
    GPU_TEXTURE_WRAP_PARAM wrap;
    GPU_TESTFUNC compare;
    GPU_BLENDEQUATION equation;
    GPU_BLENDFACTOR source,destination;
    int alphaRef,writeMask;
    bool alphaTest,twoSided,blended;
};
struct picaPreparedMaterialCounters {
    unsigned stateChanges=0,textureRequests=0,textureChanges=0;
};
// SDK state and counters belong to the consumer. Caller applies the captured
// culling/write mask to its raster-state shadow, in the original pass order.
// Keep the record alive until the next bind/fence: Citro3D retains its address.
inline void picaApplyPreparedMaterialPass(const picaPreparedMaterialPass& input,
    picaStateCache& cache,picaPreparedMaterialCounters& counters,C3D_Tex* liveSynchronousTexture=nullptr,C3D_Tex* ownedStorage=nullptr)
{
    const bool textured=input.textureIdentity!=nullptr;
    if(textured)
    {
        // Retained versions are immutable. Normalize the consumer's descriptor,
        // preserving the original mutable-record path for synchronous callers.
        if(ownedStorage)memcpy(ownedStorage,&input.texture,sizeof(input.texture));
        C3D_Tex& texture=ownedStorage?*ownedStorage:const_cast<C3D_Tex&>(input.texture);
        C3D_TexSetFilter(&texture,input.filter,input.filter);
        C3D_TexSetWrap(&texture,input.wrap,input.wrap);
        ++counters.textureRequests;
        if(!cache.texValid || cache.texPtr!=input.textureIdentity || cache.texData!=texture.data ||
           cache.texParam!=texture.param || cache.texWidth!=texture.width ||
           cache.texHeight!=texture.height || cache.texFmt!=texture.fmt)
        {
            // Legacy SDK state aliases the asset, including later mutations even
            // without a new material pass. Synchronous integration preserves that
            // alias. A future queued consumer retains owned records and fences all
            // asset mutation instead; it must never use this synchronous argument.
            C3D_TexBind(0,liveSynchronousTexture?liveSynchronousTexture:ownedStorage?ownedStorage:&texture);
            cache.texValid=true;cache.texPtr=input.textureIdentity;cache.texData=texture.data;
            cache.texParam=texture.param;cache.texWidth=texture.width;cache.texHeight=texture.height;cache.texFmt=texture.fmt;
            ++counters.textureChanges;
        }
    }
    const int mode=textured?1:2;
    if(cache.env0Mode!=mode)
    {
        C3D_TexEnv* env=C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env,C3D_Both,textured?GPU_TEXTURE0:GPU_PRIMARY_COLOR,GPU_PRIMARY_COLOR,GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env,C3D_Both,textured?GPU_MODULATE:GPU_REPLACE);
        cache.env0Mode=mode;
    }
    if(!cache.passStagesSet)
    {
        for(int stage=1;stage<6;++stage)
        {
            C3D_TexEnv* env=C3D_GetTexEnv(stage);
            C3D_TexEnvInit(env);
            C3D_TexEnvSrc(env,C3D_Both,GPU_PREVIOUS,GPU_PREVIOUS,GPU_PREVIOUS);
            C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
        }
        cache.passStagesSet=true;
    }
    if(!cache.alphaTestValid || cache.alphaTestEnable!=input.alphaTest ||
       cache.alphaTestFunc!=input.compare || cache.alphaTestRef!=input.alphaRef)
    {
        C3D_AlphaTest(input.alphaTest,input.compare,input.alphaRef);
        cache.alphaTestValid=true;cache.alphaTestEnable=input.alphaTest;
        cache.alphaTestFunc=input.compare;cache.alphaTestRef=input.alphaRef;
        ++counters.stateChanges;
    }
    if(!cache.blendValid || cache.blendColorEq!=input.equation || cache.blendAlphaEq!=input.equation ||
       cache.blendSrcClr!=input.source || cache.blendSrcAlpha!=input.source ||
       cache.blendDstClr!=input.destination || cache.blendDstAlpha!=input.destination)
    {
        C3D_AlphaBlend(input.equation,input.equation,input.source,input.destination,input.source,input.destination);
        cache.blendValid=true;cache.blendColorEq=cache.blendAlphaEq=input.equation;
        cache.blendSrcClr=cache.blendSrcAlpha=input.source;cache.blendDstClr=cache.blendDstAlpha=input.destination;
        ++counters.stateChanges;
    }
}
#endif
