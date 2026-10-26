#ifndef OLD3DS_LANDMARK_H
#define OLD3DS_LANDMARK_H
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace Old3dsLandmark {
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
// Experimental render-only exception. Normal scene distance remains unchanged.
extern bool enabled;
constexpr float ExtraDistance=80.0f;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
extern unsigned selected,rootPlaced,rootFallback,visible;
#endif
#ifndef OLD3DS_LANDMARK_PROFILE_PATH
#define OLD3DS_LANDMARK_PROFILE_PATH "romfs:/landmark-visibility.bin"
#endif
struct Record { std::uint32_t name,geometry; };
struct PreparedProfile {
    static constexpr unsigned Capacity=448;
    Record records[Capacity]{};
    unsigned count=0;
    bool attempted=false;

    // Read once during asset loading. Missing or malformed profiles fail closed.
    void Load(const char* path)
    {
        if(attempted) return;
        attempted=true;
        FILE* file=std::fopen(path,"rb");
        if(!file) return;
        std::uint32_t header[4];
        bool valid=std::fread(header,sizeof(header),1,file)==1 &&
            !std::memcmp(header,"LMV1",4) && header[1]==1 &&
            header[2]<=Capacity && header[3]==sizeof(Record);
        const unsigned length=valid?header[2]:0;
        if(valid) valid=std::fread(records,sizeof(Record),length,file)==length &&
            std::fgetc(file)==EOF && !std::ferror(file);
        std::fclose(file);
        for(unsigned i=1;valid && i<length;++i)
            valid=records[i-1].name<records[i].name ||
                (records[i-1].name==records[i].name && records[i-1].geometry<records[i].geometry);
        if(valid) count=length;
    }
    bool Contains(std::uint32_t name,std::uint32_t geometry) const
    {
        unsigned low=0,high=count;
        while(low<high) {
            const unsigned middle=low+(high-low)/2;
            const Record& record=records[middle];
            if(record.name<name || (record.name==name && record.geometry<geometry)) low=middle+1;
            else high=middle;
        }
        return low<count && records[low].name==name && records[low].geometry==geometry;
    }
};
inline PreparedProfile& ProfileCache() { static PreparedProfile profile; return profile; }
inline std::uint32_t HashBytes(const void* data,unsigned size,std::uint32_t hash=2166136261u)
{
    const unsigned char* bytes=static_cast<const unsigned char*>(data);
    for(unsigned i=0;i<size;++i) hash=(hash^bytes[i])*16777619u;
    return hash;
}
inline std::uint32_t NameKey(const char* file,const char* name)
{
    const char* base=file;
    for(const char* p=file;*p;++p) if(*p=='/' || *p=='\\') base=p+1;
    std::uint32_t hash=2166136261u;
    for(;*base;++base) {
        unsigned char c=static_cast<unsigned char>(*base);
        if(c>='A' && c<='Z') c=static_cast<unsigned char>(c-'A'+'a');
        hash=(hash^c)*16777619u;
    }
    const unsigned char separator=0;
    return HashBytes(name,std::strlen(name),HashBytes(&separator,1,hash));
}
inline bool SelectStatic(const char* file,const char* name,const float (&bounds)[10])
{
    if(!enabled || !file || !name) return false;
    PreparedProfile& profile=ProfileCache();
    profile.Load(OLD3DS_LANDMARK_PROFILE_PATH);
    return profile.Contains(NameKey(file,name),HashBytes(bounds,sizeof(bounds)));
}
#endif
}
#endif
