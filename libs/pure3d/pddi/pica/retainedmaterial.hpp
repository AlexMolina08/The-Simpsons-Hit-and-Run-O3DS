#ifndef PICA_RETAINED_MATERIAL_HPP
#define PICA_RETAINED_MATERIAL_HPP
#include <pddi/pica/preparedmaterial.hpp>
#include <pddi/pica/prepareduniforms.hpp>

// Main owns references and retirement. Consumer reads only immutable pass and
// uniform data. A writer replaces a queued version instead of joining core 1.
struct picaRetainedMaterial {
    picaPreparedMaterialPass pass;
    picaPreparedMaterialUniforms uniforms;
    const void* textureOwner=nullptr;
    unsigned textureRevision=0,uid=0,references=1;
    bool valid=false;
    void Retain(){++references;}
    void Release(){if(!--references)delete this;}
};
extern bool g_picaRetainedMaterialEnabled;
extern unsigned g_picaRetainedMaterialBuilds,g_picaRetainedMaterialReuses,g_picaRetainedMaterialFallbacks;
#endif
