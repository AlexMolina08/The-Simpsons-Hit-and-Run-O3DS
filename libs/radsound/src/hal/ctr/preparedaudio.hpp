#ifndef CTR_PREPARED_AUDIO_HPP
#define CTR_PREPARED_AUDIO_HPP

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Optional, bounded PC preparation. Keep the complete PCM key so a cache hit
// requires byte identity, rather than accepting a hash collision. All I/O and
// validation happens at sound-system initialization, never in a clip callback.
namespace ctrPreparedAudio {
constexpr unsigned Limit=2*1024*1024, MaxEntries=256, HeaderBytes=32, EntryBytes=16;
inline unsigned Read32(const unsigned char* p)
{
    return unsigned(p[0]) | unsigned(p[1])<<8 | unsigned(p[2])<<16 | unsigned(p[3])<<24;
}
inline unsigned Checksum(const unsigned char* data,unsigned bytes)
{
    unsigned value=2166136261u;
    for(unsigned i=0;i<bytes;++i)value=(value^data[i])*16777619u;
    return value;
}
class Cache
{
    unsigned char* data;
    unsigned size,count;
    Cache(const Cache&)=delete;
    Cache& operator=(const Cache&)=delete;
public:
    unsigned hits,misses,comparedBytes;
    Cache():data(nullptr),size(0),count(0),hits(0),misses(0),comparedBytes(0){}
    ~Cache(){Clear();}
    void Clear(){std::free(data);data=nullptr;size=count=0;}
    unsigned Bytes()const{return size;}
    unsigned Entries()const{return count;}
    bool Load(const char* path)
    {
        Clear();
        FILE* file=std::fopen(path,"rb");
        if(!file)return false;
        const bool seek=std::fseek(file,0,SEEK_END)==0;
        const long length=seek?std::ftell(file):-1;
        if(length<long(HeaderBytes) || length>long(Limit) || std::fseek(file,0,SEEK_SET)!=0)
        {std::fclose(file);return false;}
        unsigned char* loaded=static_cast<unsigned char*>(std::malloc(unsigned(length)));
        const bool read=loaded && std::fread(loaded,1,unsigned(length),file)==unsigned(length);
        std::fclose(file);
        if(!read){std::free(loaded);return false;}
        const unsigned n=Read32(loaded+12);
        bool valid=!std::memcmp(loaded,"SRRDSP1\0",8) && Read32(loaded+8)==1 &&
            n>0 && n<=MaxEntries && Read32(loaded+16)==unsigned(length) &&
            Read32(loaded+24)==3 && Read32(loaded+28)==0 &&
            HeaderBytes+n*EntryBytes<=unsigned(length) &&
            Checksum(loaded+HeaderBytes,unsigned(length)-HeaderBytes)==Read32(loaded+20);
        unsigned previous=0;
        for(unsigned i=0;valid && i<n;++i)
        {
            const unsigned char* entry=loaded+HeaderBytes+i*EntryBytes;
            const unsigned samples=Read32(entry),pcm=Read32(entry+4),dsp=Read32(entry+8),bytes=Read32(entry+12);
            const unsigned start=HeaderBytes+n*EntryBytes;
            valid=samples>=8192 && samples<=441000 && samples>=previous &&
                pcm>=start && pcm<=unsigned(length) && samples*2<=unsigned(length)-pcm &&
                dsp>=start && dsp<=unsigned(length) && bytes<=unsigned(length)-dsp &&
                bytes==((samples+13)/14)*8;
            for(unsigned byte=0;valid && byte<bytes;byte+=8)
                valid=(loaded[dsp+byte]>>4)<=2 && (loaded[dsp+byte]&15)<=12;
            previous=samples;
        }
        if(!valid){std::free(loaded);return false;}
        data=loaded;size=unsigned(length);count=n;
        return true;
    }
    bool Copy(void* pcm,unsigned samples,unsigned& bytes)
    {
        if(!data || !pcm)return false;
        unsigned first=0,last=count;
        while(first<last)
        {
            const unsigned middle=first+(last-first)/2;
            if(Read32(data+HeaderBytes+middle*EntryBytes)<samples)first=middle+1;
            else last=middle;
        }
        for(unsigned i=first;i<count;++i)
        {
            const unsigned char* entry=data+HeaderBytes+i*EntryBytes;
            if(Read32(entry)!=samples)break;
            const unsigned char* original=data+Read32(entry+4);
            // Cheap rejection before comparing an entire long sound clip.
            if(std::memcmp(pcm,original,16) ||
               std::memcmp(static_cast<const unsigned char*>(pcm)+samples*2-16,original+samples*2-16,16))continue;
            comparedBytes+=samples*2;
            if(std::memcmp(pcm,original,samples*2))continue;
            bytes=Read32(entry+12);
            std::memcpy(pcm,data+Read32(entry+8),bytes);
            ++hits;
            return true;
        }
        ++misses;
        return false;
    }
};
}
#endif
