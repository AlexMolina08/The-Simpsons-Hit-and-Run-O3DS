#ifndef PICA_CHARACTEROCCLUSION_HPP
#define PICA_CHARACTEROCCLUSION_HPP
#include <cstring>
#include <pddi/pica/skincore.hpp>
#include <pddi/pica/visualstyle.hpp>

namespace picaCharacterOcclusion {
inline unsigned Word(const unsigned char* value)
{
    return unsigned(value[0]) | unsigned(value[1])<<8 | unsigned(value[2])<<16 | unsigned(value[3])<<24;
}
inline unsigned Hash(const unsigned char* value, unsigned count, unsigned hash=2166136261u)
{
    for(unsigned i=0;i<count;++i) hash=(hash^value[i])*16777619u;
    return hash;
}
// A content mismatch leaves colors untouched. Only source normal/position bytes
// participate, so weights, palettes and animation never change the baked lookup.
inline const unsigned char* Find(const unsigned char* data, unsigned size, const char* name,
                                 unsigned group, const void* source, unsigned count)
{
    if(!data || size<16 || !name || !source || !count || memcmp(data,"SRRAO001",8) || Word(data+12)!=size)
        return nullptr;
    const unsigned records=Word(data+8);
    if(records>(size-16)/20) return nullptr;
    unsigned signature=2166136261u;
    const unsigned samples[]={0,count/2,count-1};
    for(unsigned sample:samples)
    {
        picaSkin::Source vertex;
        memcpy(&vertex,static_cast<const unsigned char*>(source)+sample*sizeof(vertex),sizeof(vertex));
        signature=Hash(reinterpret_cast<const unsigned char*>(vertex.position),12,signature);
        signature=Hash(reinterpret_cast<const unsigned char*>(vertex.normal),12,signature);
    }
    const unsigned key[]={Hash(reinterpret_cast<const unsigned char*>(name),strlen(name)),group,count,signature};
    unsigned first=0,last=records;
    while(first<last)
    {
        const unsigned middle=first+(last-first)/2;
        const unsigned char* record=data+16+20*middle;
        int order=0;
        for(unsigned i=0;i<4 && !order;++i)
        {
            const unsigned value=Word(record+4*i);
            order=value<key[i]?-1:value>key[i]?1:0;
        }
        if(order<0) first=middle+1;
        else if(order>0) last=middle;
        else
        {
            const unsigned offset=Word(record+16);
            return offset>=16+20*records && offset<=size && count<=size-offset ? data+offset : nullptr;
        }
    }
    return nullptr;
}
inline void Apply(unsigned char* colour, unsigned shade)
{
    for(unsigned channel=0;channel<3;++channel)
        colour[channel]=static_cast<unsigned char>((unsigned(colour[channel])*shade+127)/255);
}
}
#endif
