#ifndef PICA_NATIVE_READ_AHEAD_HPP
#define PICA_NATIVE_READ_AHEAD_HPP
#include <cstdlib>
#include <cstring>

namespace picaNativeRead {
constexpr unsigned BufferBytes=32768;
struct Hooks {
    void* (*open)(const char*,unsigned)=nullptr;
    bool (*read)(void*,unsigned,void*,unsigned)=nullptr;
    void (*close)(void*)=nullptr;
};
extern Hooks hooks;
extern bool enabled;

// One owned raw buffer and one in-flight range. The bridge uses the existing
// file-I/O thread and releases the engine mutex while waiting. Consumption and
// all descriptor/body validation resume in the protected loader path.
class Stream {
    Hooks bridge;
    void* file=nullptr;
    unsigned char* buffer=nullptr;
    unsigned position=0,remaining=0,available=0,cursor=0;
    bool failed=false;
public:
    Stream(Hooks selected,const char* path,unsigned bundleBytes,unsigned offset,unsigned bytes):bridge(selected)
    {
        if(!bridge.open || !bridge.read || !bridge.close || offset>bundleBytes || bytes>bundleBytes-offset)return;
        buffer=static_cast<unsigned char*>(std::malloc(BufferBytes));
        if(!buffer)return;
        file=bridge.open(path,bundleBytes);position=offset;remaining=bytes;
    }
    ~Stream(){if(file)bridge.close(file);std::free(buffer);}
    Stream(const Stream&)=delete;
    Stream& operator=(const Stream&)=delete;
    bool Ready()const{return file&&buffer;}
    bool Failed()const{return failed;}
    size_t Read(void* output,size_t bytes)
    {
        if(!Ready() || failed)return 0;
        auto* destination=static_cast<unsigned char*>(output);size_t copied=0;
        while(bytes){
            if(cursor==available){
                if(!remaining)break;
                available=remaining<BufferBytes?remaining:BufferBytes;cursor=0;
                if(!bridge.read(file,position,buffer,available)){failed=true;return copied;}
                position+=available;remaining-=available;
            }
            const size_t count=bytes<available-cursor?bytes:available-cursor;
            std::memcpy(destination+copied,buffer+cursor,count);
            cursor+=unsigned(count);copied+=count;bytes-=count;
        }
        return copied;
    }
};
}
#endif
