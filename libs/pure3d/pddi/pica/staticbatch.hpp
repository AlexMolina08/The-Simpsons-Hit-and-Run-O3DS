#ifndef PICA_STATIC_BATCH_HPP
#define PICA_STATIC_BATCH_HPP
#include <cstdint>
#include <cstring>
namespace picaStaticBatch {
constexpr unsigned Capacity=16,Slots=8;
constexpr unsigned GroupLimit=64*1024,Budget=256*1024,Reserve=2*1024*1024;
// Each entry retains at most GroupLimit source bytes as well as its baked copy.
// Eight entries therefore retain at most 512 KiB of original allocations.
inline unsigned Triangles(unsigned count,bool strip)
{
    return strip ? (count>=3 ? count-2 : 0) : (count && count%3==0 ? count/3 : 0);
}
inline bool AppendIndices(unsigned short* output,const unsigned short* source,unsigned count,
                          unsigned vertices,unsigned base,bool strip)
{
    if(!output || !source || !vertices || vertices>65536u || base>65536u-vertices) return false;
    const unsigned triangles=Triangles(count,strip);
    if(!triangles) return false;
    for(unsigned t=0;t<triangles;++t)
    {
        unsigned corners[3]={strip?t:t*3,strip?t+1:t*3+1,strip?t+2:t*3+2};
        // Preserve winding and the final provoking corner, including degenerates.
        if(strip && (t&1)) {unsigned swap=corners[0];corners[0]=corners[1];corners[1]=swap;}
        for(unsigned c=0;c<3;++c)
        {
            if(source[corners[c]]>=vertices) return false;
            output[t*3+c]=static_cast<unsigned short>(base+source[corners[c]]);
        }
    }
    return true;
}
inline void CopyVertices(unsigned char* output,const unsigned char* source,unsigned count,bool packed)
{
    if(!packed) {std::memcpy(output,source,count*36);return;}
    const float normal[3]={0,0,1};
    for(unsigned v=0;v<count;++v)
    {
        std::memcpy(output+v*36,source+v*24,12);
        std::memcpy(output+v*36+12,normal,12);
        std::memcpy(output+v*36+24,source+v*24+12,12);
    }
}
template<class Buffer> struct Entry {
    Buffer* sources[Capacity]={};
    unsigned revisions[Capacity]={};
    Buffer* draw=nullptr;
    unsigned count=0,bytes=0,seen=0,lastFrame=0;
    bool Matches(Buffer* const* input,unsigned size) const
    {
        if(size!=count) return false;
        for(unsigned i=0;i<size;++i)
            if(sources[i]!=input[i] || revisions[i]==~0u || revisions[i]!=input[i]->WheelSourceVersion()) return false;
        return true;
    }
    void Reset()
    {
        if(draw) draw->Release();
        for(unsigned i=0;i<count;++i) sources[i]->Release();
        draw=nullptr;count=bytes=seen=lastFrame=0;
    }
    void Remember(Buffer* const* input,unsigned size,unsigned frame)
    {
        count=size;seen=1;lastFrame=frame;
        for(unsigned i=0;i<size;++i)
        {
            sources[i]=input[i];sources[i]->AddRef();revisions[i]=input[i]->WheelSourceVersion();
        }
    }
};
}
#endif
