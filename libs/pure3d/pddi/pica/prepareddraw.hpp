#ifndef PICA_PREPARED_DRAW_HPP
#define PICA_PREPARED_DRAW_HPP
#include <pddi/pica/preparedbackend.h>
#include <pddi/pica/preparedmaterial.hpp>
#include <pddi/pica/preparedbatch.hpp>
#include <pddi/pica/preparedraster.hpp>
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
#include <pddi/pica/retainedmaterial.hpp>
#endif

// Stack-owned inputs are currently consumed synchronously. This is not a queue.
// Geometry, palette and resource fences must join this boundary before threading.
class picaMat;
struct picaPreparedDrawInputs {
    const picaMat* materialOwner=nullptr;
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
    picaRetainedMaterial* retained=nullptr;
#endif
    picaPreparedMaterialPass material;
    picaPreparedUniformRecord uniforms;
    picaPreparedMaterialRaster raster;
    bool hasMaterial=false,hasUniforms=false;
};
#endif
