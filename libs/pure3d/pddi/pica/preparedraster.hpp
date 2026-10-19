#ifndef PICA_PREPARED_RASTER_HPP
#define PICA_PREPARED_RASTER_HPP
#include <citro3d.h>
#include <pddi/pica/statecache.hpp>

// Values sampled where SetDevPass originally applied culling and write masks.
// No consumer reads the producer's PDDI render state or matWriteMask.
struct picaPreparedMaterialRaster {
    GPU_CULLMODE cull;
    GPU_TESTFUNC depthFunction;
    GPU_WRITEMASK depthMask;
    bool depthEnabled,applyDepth;
};
inline unsigned picaApplyPreparedMaterialRaster(const picaPreparedMaterialRaster& input,picaStateCache& cache)
{
    unsigned changes=0;
    if(!cache.cullValid || cache.cull!=input.cull)
    {
        C3D_CullFace(input.cull);
        cache.cullValid=true;cache.cull=input.cull;++changes;
    }
    if(input.applyDepth && (!cache.depthValid || cache.depthEnable!=input.depthEnabled ||
       cache.depthFunc!=input.depthFunction || cache.depthMask!=input.depthMask))
    {
        C3D_DepthTest(input.depthEnabled,input.depthFunction,input.depthMask);
        cache.depthValid=true;cache.depthEnable=input.depthEnabled;
        cache.depthFunc=input.depthFunction;cache.depthMask=input.depthMask;++changes;
    }
    return changes;
}
#endif
