#ifndef PICA_NATIVE_TEXTURE_HPP
#define PICA_NATIVE_TEXTURE_HPP
#include <cstdint>
#include <cstdio>
#include <cstring>

// Versioned little-endian file fields, never SDK structures or pointers.
// Bump Version whenever conversion, filtering, mip rounding or tiling changes.
namespace picaNativeTexture {
const unsigned Version=1, HeaderBytes=108;
const unsigned EncodedPNG=256;
enum Format { RGB565=1, RGBA5551=2, RGBA4=3, ETC1=4 };
inline bool WorldSource(const char* path)
{
    if(!path)return false;
    const char* name=path;const char* parent=nullptr;
    for(const char* p=path;*p;++p)if(*p=='/' || *p=='\\'){parent=name;name=p+1;}
    if(!parent || name-parent!=4 || (parent[0]!='a' && parent[0]!='A') ||
       (parent[1]!='r' && parent[1]!='R') || (parent[2]!='t' && parent[2]!='T'))return false;
    const unsigned length=unsigned(strlen(name));
    if(length<8 || (name[0]!='l' && name[0]!='L') || name[1]<'1' || name[1]>'7')return false;
    const char* suffix=name+length-4;
    if(suffix[0]!='.' || (suffix[1]!='p' && suffix[1]!='P') || suffix[2]!='3' ||
       (suffix[3]!='d' && suffix[3]!='D'))return false;
    if(name[2]=='_'){
        static const char terra[]="_terra.p3d";
        if(length!=12)return false;
        for(unsigned i=0;i<10;++i){char c=name[2+i];if(c>='A' && c<='Z')c+=32;if(c!=terra[i])return false;}
        return true;
    }
    return (name[2]=='i' || name[2]=='I' || name[2]=='r' || name[2]=='R' ||
            name[2]=='z' || name[2]=='Z') && name[3]>='0' && name[3]<='9';
}
struct Description {
    unsigned sourceWidth,sourceHeight,kind,width,height,format,levels,sourceBytes,bytes;
};
inline void Put(unsigned char* p,unsigned value)
{for(unsigned i=0;i<4;++i)p[i]=static_cast<unsigned char>(value>>(i*8));}
inline void Encode(const Description& d,unsigned char* out)
{
    const unsigned words[]={0x3158544eu,d.format==ETC1?2u:Version,d.sourceWidth,d.sourceHeight,d.kind,
        d.width,d.height,d.format,d.levels,d.sourceBytes,d.bytes};
    for(unsigned i=0;i<11;++i)Put(out+4*i,words[i]);
}
template<class Hash> bool HeaderKey(Hash& hash,const unsigned char* header,char* hex)
{
    unsigned char key[32];
    if(!hash(header,76,key))return false;
    static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<32;++i){hex[2*i]=digits[key[i]>>4];hex[2*i+1]=digits[key[i]&15];}
    hex[64]=0;return true;
}
template<class Hash> bool Key(const Description& d,const void* source,Hash& hash,
                             unsigned char* header,char* hex)
{
    Encode(d,header);
    if(!hash(source,d.sourceBytes,header+44))return false;
    return HeaderKey(hash,header,hex);
}
// Only a first, unpublished allocation may lend its unused tail to wire bytes.
// Keep source and destination disjoint and bound each filesystem request.
inline unsigned char* RecordScratch(void* allocation,unsigned capacity,unsigned bodyBytes,
                                    unsigned recordBytes,bool unpublished)
{
    if(!unpublished || !allocation || !bodyBytes || bodyBytes>capacity ||
       recordBytes<HeaderBytes || recordBytes>32768 ||
       recordBytes-HeaderBytes!=bodyBytes || recordBytes>capacity-bodyBytes)return nullptr;
    return static_cast<unsigned char*>(allocation)+capacity-recordBytes;
}
struct MemorySource {
    const unsigned char* data;unsigned size,cursor=0;
    size_t Read(void* output,size_t bytes) {
        const size_t count=bytes<size-cursor?bytes:size-cursor;
        if(count)std::memcpy(output,data+cursor,count);
        cursor+=unsigned(count);return count;
    }
    bool Failed()const{return false;}
};
template<class Source,class Hash> bool ReadSource(Source& source,const unsigned char* expected,unsigned bytes,
                              void* destination,Hash& hash,void (*yield)()=nullptr,bool exactFile=true)
{
    unsigned char header[HeaderBytes],digest[32];
    // Validate the allocation-specific descriptor and source content before
    // touching the destination. An incomplete/corrupt body is never published.
    if(source.Read(header,sizeof(header))!=sizeof(header) || memcmp(header,expected,76))return false;
    unsigned offset=0;
    while(offset<bytes)
    {
        const unsigned remaining=bytes-offset,chunk=remaining<32768?remaining:32768;
        if(source.Read(static_cast<unsigned char*>(destination)+offset,chunk)!=chunk)return false;
        offset+=chunk;if(yield)yield();
    }
    unsigned char extra;
    if((exactFile && source.Read(&extra,1)!=0) || source.Failed())return false;
    return hash(destination,bytes,digest) && !memcmp(digest,header+76,32);
}
template<class Hash> bool Read(FILE* f,const unsigned char* expected,unsigned bytes,
                              void* destination,Hash& hash,void (*yield)()=nullptr,bool exactFile=true)
{
    struct FileSource {
        FILE* file;
        size_t Read(void* output,size_t count){return fread(output,1,count,file);}
        bool Failed()const{return ferror(file)!=0;}
    } source{f};
    return ReadSource(source,expected,bytes,destination,hash,yield,exactFile);
}
}
#endif
