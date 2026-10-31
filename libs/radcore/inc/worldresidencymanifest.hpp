#ifndef SRR_WORLD_RESIDENCY_MANIFEST_HPP
#define SRR_WORLD_RESIDENCY_MANIFEST_HPP
#include <worldresidencyassets.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace WorldResidencyAssets {
constexpr unsigned ArtCount=sizeof(Files)/sizeof(Files[0]);
constexpr unsigned ManifestCount=ArtCount+8;
struct AssetRecord {char path[48];std::uint32_t bytes;unsigned char sha256[32];};
static_assert(sizeof(AssetRecord)==84,"resident asset manifest wire stride");
inline bool AssetPath(unsigned index,char (&path)[48]) {
    int size=-1;
    if(index<ArtCount)size=std::snprintf(path,sizeof(path),"art/%s",Files[index]);
    else if(index<ArtCount+7)size=std::snprintf(path,sizeof(path),"resident-visibility%u.bin",index-ArtCount+1);
    else if(index==ArtCount+7)size=std::snprintf(path,sizeof(path),"landmark-visibility.bin");
    return size>0 && unsigned(size)<sizeof(path);
}
struct Manifest {
    unsigned char header[80];AssetRecord records[ManifestCount];
    template<class Hash> bool Validate(Hash& hash,const char* nativeBundleHash) const {
        unsigned char bodyHash[32];if(!hash(records,sizeof(records),bodyHash))return false;
        std::uint32_t words[4];std::memcpy(words,header,sizeof(words));
        if(std::memcmp(header,"WRA1",4) || words[1]!=1 || words[2]!=ManifestCount || words[3]!=sizeof(AssetRecord)
           || std::memcmp(header+48,bodyHash,32) || !nativeBundleHash || std::strlen(nativeBundleHash)!=64)return false;
        const char hex[]="0123456789abcdef";
        for(unsigned i=0;i<32;++i)if(nativeBundleHash[i*2]!=hex[header[16+i]>>4] || nativeBundleHash[i*2+1]!=hex[header[16+i]&15])return false;
        for(unsigned i=0;i<ManifestCount;++i){
            const auto& record=records[i];const char* end=static_cast<const char*>(std::memchr(record.path,0,sizeof(record.path)));
            if(!end || end==record.path || !record.bytes || record.bytes>64u*1024*1024)return false;
            for(const char* cursor=end;cursor<record.path+sizeof(record.path);++cursor)if(*cursor)return false;
            if(i && std::strcmp(records[i-1].path,record.path)>=0)return false;
            bool known=false;
            for(unsigned n=0;n<ManifestCount;++n){char path[48];if(!AssetPath(n,path))return false;known|=!std::strcmp(path,record.path);}
            if(!known)return false;
        }
        return true;
    }
    const AssetRecord* Find(const char* path) const {
        if(!path)return nullptr;
        unsigned low=0,high=ManifestCount;
        while(low<high){unsigned middle=low+(high-low)/2;if(std::strcmp(records[middle].path,path)<0)low=middle+1;else high=middle;}
        return low<ManifestCount && !std::strcmp(records[low].path,path)?records+low:nullptr;
    }
};
}
#endif
