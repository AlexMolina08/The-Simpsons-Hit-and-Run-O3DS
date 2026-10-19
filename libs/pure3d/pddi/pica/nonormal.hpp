#ifndef PICA_NO_NORMAL_HPP
#define PICA_NO_NORMAL_HPP
#include <cstring>
namespace picaNoNormal {
const unsigned Stride = 24;
// Keep allocation capacity and indices unchanged. Expansion needs no allocation,
// so a later edit can always recover the exact ordinary stream representation.
inline void Pack(unsigned char* data, unsigned count) {
    for(unsigned i=0;i<count;++i) {
        std::memmove(data+i*Stride, data+i*36, 12);
        std::memmove(data+i*Stride+12, data+i*36+24, 12);
    }
}
inline void Expand(unsigned char* data, unsigned count) {
    const float normal[3]={0.0f,0.0f,1.0f};
    while(count) {
        --count; unsigned char vertex[Stride];
        std::memcpy(vertex,data+count*Stride,Stride);
        std::memcpy(data+count*36,vertex,12);
        std::memcpy(data+count*36+12,normal,12);
        std::memcpy(data+count*36+24,vertex+12,12);
    }
}
}
#endif
