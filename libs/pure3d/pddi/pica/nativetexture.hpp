#ifndef PICA_NATIVE_TEXTURE_HPP
#define PICA_NATIVE_TEXTURE_HPP
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

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
template<class Hash> bool Key(const Description& d,const void* source,Hash& hash,
                             unsigned char* header,char* hex)
{
    Encode(d,header);
    if(!hash(source,d.sourceBytes,header+44))return false;
    unsigned char key[32];
    if(!hash(header,76,key))return false;
    static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<32;++i){hex[2*i]=digits[key[i]>>4];hex[2*i+1]=digits[key[i]&15];}
    hex[64]=0;return true;
}
template<class Hash> bool Read(FILE* f,const unsigned char* expected,unsigned bytes,
                              void* destination,Hash& hash,void (*yield)()=nullptr,bool exactFile=true)
{
    unsigned char header[HeaderBytes],digest[32];
    // Validate the allocation-specific descriptor and source content before
    // touching the destination. An incomplete/corrupt body is never published.
    if(fread(header,1,sizeof(header),f)!=sizeof(header) || memcmp(header,expected,76))return false;
    unsigned offset=0;
    while(offset<bytes)
    {
        const unsigned remaining=bytes-offset,chunk=remaining<32768?remaining:32768;
        if(fread(static_cast<unsigned char*>(destination)+offset,1,chunk,f)!=chunk)return false;
        offset+=chunk;if(yield)yield();
    }
    if((exactFile && fgetc(f)!=EOF) || ferror(f))return false;
    return hash(destination,bytes,digest) && !memcmp(digest,header+76,32);
}
const unsigned ContiguousLimit=65536;
// Optional bounded input staging keeps header and body in one read request.
// Verify the same descriptor and digest before publishing any destination bytes.
template<class Hash> bool ReadContiguous(FILE* f,const unsigned char* expected,unsigned bytes,
    void* destination,Hash& hash,void (*yield)()=nullptr,bool exactFile=true,
    bool* usedBlock=nullptr,void* (*allocate)(size_t)=std::malloc)
{
    if(usedBlock)*usedBlock=false;
    unsigned char* block=bytes<=ContiguousLimit
        ?static_cast<unsigned char*>(allocate(HeaderBytes+bytes)):nullptr;
    if(!block)return Read(f,expected,bytes,destination,hash,yield,exactFile);
    struct OwnedBlock {unsigned char* data;~OwnedBlock(){std::free(data);}} owned{block};
    if(usedBlock)*usedBlock=true;
    unsigned char digest[32];
    bool valid=fread(block,1,HeaderBytes+bytes,f)==HeaderBytes+bytes && !memcmp(block,expected,76);
    if(valid)valid=(!exactFile || fgetc(f)==EOF) && !ferror(f);
    if(valid){
        if(yield)yield();
        valid=hash(block+HeaderBytes,bytes,digest) && !memcmp(digest,block+76,32);
        if(valid)std::memcpy(destination,block+HeaderBytes,bytes);
    }
    return valid;
}
}
#endif
