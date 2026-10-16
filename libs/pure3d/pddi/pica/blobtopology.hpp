#ifndef PICA_BLOB_TOPOLOGY_HPP
#define PICA_BLOB_TOPOLOGY_HPP

// Vertex 0 is the centre. Each perimeter point stores its inner and outer vertex.
// Preserve the original triangle-list interior, then the strip's alternating
// winding. The closing strip vertices reference point zero instead of copies.
namespace picaBlob
{
    const unsigned VertexCount = 25;
    const unsigned IndexCount = 108;
    inline void Indices(unsigned short* indices)
    {
        unsigned n = 0;
        for(unsigned i = 0; i < 12; ++i)
        {
            indices[n++] = 0;
            indices[n++] = 1 + 2 * ((i + 1) % 12);
            indices[n++] = 1 + 2 * i;
        }
        for(unsigned i = 0; i < 12; ++i)
        {
            const unsigned inner = 1 + 2 * i, outer = inner + 1;
            const unsigned nextInner = 1 + 2 * ((i + 1) % 12), nextOuter = nextInner + 1;
            indices[n++] = outer; indices[n++] = inner; indices[n++] = nextOuter;
            indices[n++] = nextOuter; indices[n++] = inner; indices[n++] = nextInner;
        }
    }
}
#endif
