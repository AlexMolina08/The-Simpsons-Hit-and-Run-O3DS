#ifndef PICA_QUADSTREAM_HPP
#define PICA_QUADSTREAM_HPP
#include <pddi/pddi.hpp>
namespace picaQuad {
enum Topology { Billboard=0, Flipped=1, Particle=2, Sparkle=3 };
const unsigned MaxQuads=16384;
inline bool Indices(unsigned short* output,unsigned quad,unsigned topology)
{
    if(quad>=MaxQuads || unsigned(topology)>3) return false;
    static const unsigned char order[4][6]={{0,2,1,3,2,0},{0,1,2,0,2,3},{0,1,2,0,3,1},{0,1,2,3,2,1}};
    for(unsigned i=0;i<6;++i) output[i]=static_cast<unsigned short>(quad*4+order[topology][i]);
    return true;
}
}
#ifdef SRR2_OLD3DS_QUAD_STREAM
extern "C" pddiPrimStream* picaBeginQuadStream(pddiShader* shader,unsigned count);
extern "C" bool picaEmitQuad(pddiPrimStream* stream,const pddiVector* positions,
    const pddiVector2* coordinates,pddiColour colour,picaQuad::Topology topology);
extern "C" bool picaEmitColourQuad(pddiPrimStream* stream,const pddiVector* positions,
    const pddiVector2* coordinates,const pddiColour* colours,picaQuad::Topology topology);
#endif
#endif
