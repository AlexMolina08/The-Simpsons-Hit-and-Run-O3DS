#ifndef PICA_PREPARED_STATE_HPP
#define PICA_PREPARED_STATE_HPP
#include <pddi/pica/statecache.hpp>
#include <pddi/pica/preparedbackend.h>
struct picaPreparedState {
    unsigned kind,a,b,c,d,e,f;
};
inline unsigned picaApplyPreparedState(const picaPreparedState& input,picaStateCache& state)
{
    switch(input.kind)
    {
    case PICA_PREPARED_VIEWPORT:
        C3D_SetViewport(input.a,input.b,input.c,input.d);return 0;
    case PICA_PREPARED_SCISSOR:
        C3D_SetScissor((GPU_SCISSORMODE)input.a,input.b,input.c,input.d,input.e);return 0;
    case PICA_PREPARED_INVALIDATE:
        state.alphaTestValid=state.blendValid=state.cullValid=state.depthValid=state.texValid=false;
        state.env0Mode=0;state.passStagesSet=state.bufInfoSet=false;return 0;
    case PICA_PREPARED_ALPHA_TEST:
        if(state.alphaTestValid && state.alphaTestEnable==bool(input.a) && state.alphaTestFunc==(GPU_TESTFUNC)input.b && state.alphaTestRef==(int)input.c)return 0;
        C3D_AlphaTest(input.a,(GPU_TESTFUNC)input.b,(int)input.c);
        state.alphaTestValid=true;state.alphaTestEnable=input.a;state.alphaTestFunc=(GPU_TESTFUNC)input.b;state.alphaTestRef=(int)input.c;return 1;
    case PICA_PREPARED_BLEND:
        if(state.blendValid && state.blendColorEq==(GPU_BLENDEQUATION)input.a && state.blendAlphaEq==(GPU_BLENDEQUATION)input.b &&
           state.blendSrcClr==(GPU_BLENDFACTOR)input.c && state.blendDstClr==(GPU_BLENDFACTOR)input.d && state.blendSrcAlpha==(GPU_BLENDFACTOR)input.e && state.blendDstAlpha==(GPU_BLENDFACTOR)input.f)return 0;
        C3D_AlphaBlend((GPU_BLENDEQUATION)input.a,(GPU_BLENDEQUATION)input.b,(GPU_BLENDFACTOR)input.c,(GPU_BLENDFACTOR)input.d,(GPU_BLENDFACTOR)input.e,(GPU_BLENDFACTOR)input.f);
        state.blendValid=true;state.blendColorEq=(GPU_BLENDEQUATION)input.a;state.blendAlphaEq=(GPU_BLENDEQUATION)input.b;
        state.blendSrcClr=(GPU_BLENDFACTOR)input.c;state.blendDstClr=(GPU_BLENDFACTOR)input.d;state.blendSrcAlpha=(GPU_BLENDFACTOR)input.e;state.blendDstAlpha=(GPU_BLENDFACTOR)input.f;return 1;
    case PICA_PREPARED_CULL:
        if(state.cullValid && state.cull==(GPU_CULLMODE)input.a)return 0;
        C3D_CullFace((GPU_CULLMODE)input.a);state.cullValid=true;state.cull=(GPU_CULLMODE)input.a;return 1;
    case PICA_PREPARED_DEPTH:
        if(state.depthValid && state.depthEnable==bool(input.a) && state.depthFunc==(GPU_TESTFUNC)input.b && state.depthMask==(GPU_WRITEMASK)input.c)return 0;
        C3D_DepthTest(input.a,(GPU_TESTFUNC)input.b,(GPU_WRITEMASK)input.c);
        state.depthValid=true;state.depthEnable=input.a;state.depthFunc=(GPU_TESTFUNC)input.b;state.depthMask=(GPU_WRITEMASK)input.c;return 1;
    }
    return 0;
}
#endif
