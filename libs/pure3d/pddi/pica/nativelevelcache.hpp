#ifndef PICA_NATIVE_LEVEL_CACHE_HPP
#define PICA_NATIVE_LEVEL_CACHE_HPP
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace picaNativeLevel {
constexpr unsigned Magic=0x3143534e,HeaderBytes=64,EntryBytes=40;
constexpr unsigned MaximumBytes=8*1024*1024,MaximumRecords=2048;
struct Info {unsigned count=0,payloadOffset=0,bytes=0,level=0;};
inline unsigned Word(const unsigned char* p)
{
    return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);
}
inline bool Header(const unsigned char* h,unsigned bytes,Info& out)
{
    if(bytes!=HeaderBytes || Word(h)!=Magic || Word(h+4)!=1 ||
       !Word(h+8) || Word(h+8)>MaximumRecords || Word(h+12)!=EntryBytes ||
       Word(h+16)!=HeaderBytes+Word(h+8)*EntryBytes ||
       Word(h+20)>MaximumBytes || Word(h+20)<=Word(h+16) ||
       Word(h+24)<1 || Word(h+24)>7 || Word(h+28))return false;
    out={Word(h+8),Word(h+16),Word(h+20),Word(h+24)};
    return true;
}
inline bool Index(const Info& info,const unsigned char* table,unsigned bytes)
{
    if(!info.count || info.count>MaximumRecords || info.bytes>MaximumBytes ||
       info.payloadOffset!=HeaderBytes+info.count*EntryBytes || info.bytes<=info.payloadOffset ||
       bytes!=info.count*EntryBytes)return false;
    unsigned offset=0;
    for(unsigned i=0;i<info.count;++i) {
        const unsigned char* entry=table+i*EntryBytes;
        const unsigned size=Word(entry+36);
        if((i && std::memcmp(entry-EntryBytes,entry,32)>=0) || Word(entry+32)!=offset ||
           size<108 || offset>info.bytes-info.payloadOffset ||
           size>info.bytes-info.payloadOffset-offset)return false;
        offset+=size;
    }
    return offset==info.bytes-info.payloadOffset;
}
inline const unsigned char* Find(const Info& info,const unsigned char* table,const char* hex,unsigned bytes)
{
    unsigned char key[32];
    for(unsigned i=0;i<64;++i) {
        const char c=hex[i];
        const int value=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;
        if(value<0)return nullptr;
        if(!(i&1))key[i/2]=value<<4;else key[i/2]|=value;
    }
    unsigned first=0,last=info.count;
    while(first<last) {
        const unsigned middle=first+(last-first)/2;
        const unsigned char* entry=table+middle*EntryBytes;
        const int order=std::memcmp(key,entry,32);
        if(order<0)last=middle;else if(order>0)first=middle+1;
        else return Word(entry+36)==bytes?entry:nullptr;
    }
    return nullptr;
}
template<class Hash> bool Verify(const unsigned char* header,const Info& info,
    const unsigned char* body,Hash& hash)
{
    if(!Index(info,body,info.count*EntryBytes))return false;
    unsigned char result[32];
    if(!hash(body,info.bytes-HeaderBytes,result) || std::memcmp(result,header+32,32))return false;
    for(unsigned i=0;i<info.count;++i) {
        const unsigned char* entry=body+i*EntryBytes;
        const unsigned char* record=body+info.count*EntryBytes+Word(entry+32);
        const unsigned size=Word(entry+36);
        if(Word(record)!=0x3158544e || (Word(record+4)!=1 && Word(record+4)!=2) ||
           Word(record+40)!=size-108 || !hash(record,76,result) ||
           std::memcmp(result,entry,32) || !hash(record+108,size-108,result) ||
           std::memcmp(result,record+76,32))return false;
    }
    return true;
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
#ifdef SRR2_OLD3DS_LEVEL_CACHE
unsigned Mode();
bool SetMode(unsigned);
void Start(unsigned level);
void Tick();
void Finish();
void Dump(FILE*);
#endif
}
#endif
