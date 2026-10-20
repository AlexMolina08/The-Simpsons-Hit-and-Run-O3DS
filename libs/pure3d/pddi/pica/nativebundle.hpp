#ifndef PICA_NATIVE_BUNDLE_HPP
#define PICA_NATIVE_BUNDLE_HPP
#include <cstring>
namespace picaNativeBundle {
template<unsigned KeyBytes> struct IndexEntry {
    static_assert(KeyBytes==16 || KeyBytes==32,"Native bundle keys must use 128 or 256 bits");
    unsigned char key[KeyBytes];unsigned offset,bytes;
};
using Entry=IndexEntry<32>;
template<unsigned KeyBytes>
inline const IndexEntry<KeyBytes>* Find(const IndexEntry<KeyBytes>* entries,unsigned count,const char* hexadecimal,unsigned bytes)
{
    unsigned char key[32];
    for(unsigned i=0;i<64;++i){
        const char c=hexadecimal[i];
        const int value=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;
        if(value<0)return nullptr;
        if(!(i&1))key[i/2]=value<<4;else key[i/2]|=value;
    }
    unsigned first=0,last=count;
    while(first<last){const unsigned middle=first+(last-first)/2;
        const int order=memcmp(key,entries[middle].key,KeyBytes);
        if(order<0)last=middle;else if(order>0)first=middle+1;
        else return entries[middle].bytes==bytes?entries+middle:nullptr;
    }
    return nullptr;
}
}
#endif
