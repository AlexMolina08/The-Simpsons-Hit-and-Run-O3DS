#ifndef RAD_LOAD_TIMING_HPP
#define RAD_LOAD_TIMING_HPP
#include <cstdint>
#include <cstring>
#include <radloadtiming.hpp>
namespace radLoadTiming {
struct Slice {uint64_t start=0,ticks=0;unsigned routeMs=0,stage=0;char label[96]={};};
class State {
    const char* label=nullptr;
    uint64_t resumed=0;
    unsigned stage=0;
public:
    static const unsigned Capacity=16;
    Slice longest[Capacity];unsigned count=0,samples=0;
    void Commit(uint64_t now,unsigned routeMs){
        if(!label || !resumed || now<=resumed)return;
        ++samples;const uint64_t span=now-resumed;
        unsigned slot=0;while(slot<count && longest[slot].ticks>=span)++slot;
        if(slot<Capacity){
            const unsigned end=count<Capacity?count:Capacity-1;
            for(unsigned i=end;i>slot;--i)longest[i]=longest[i-1];
            Slice& entry=longest[slot];entry.start=resumed;entry.ticks=span;entry.routeMs=routeMs;entry.stage=stage;
            std::strncpy(entry.label,label,sizeof(entry.label)-1);entry.label[sizeof(entry.label)-1]=0;
            if(count<Capacity)++count;
        }
        resumed=0;
    }
    const char* Enter(const char* name,uint64_t now,unsigned routeMs){
        const char* previous=label;Commit(now,routeMs);label=name?name:"unnamed";resumed=now;return previous;
    }
    void Leave(const char* previous,uint64_t now,unsigned routeMs){
        Commit(now,routeMs);label=previous;resumed=label?now:0;
    }
    void Cancel(const char* previous){label=previous;resumed=0;}
    unsigned EnterStage(unsigned id,uint64_t now,unsigned routeMs){
        const unsigned previous=stage;Commit(now,routeMs);stage=id;resumed=label?now:0;return previous;
    }
    void LeaveStage(unsigned previous,uint64_t now,unsigned routeMs){
        Commit(now,routeMs);stage=previous;resumed=label?now:0;
    }
    void CancelStage(unsigned previous){stage=previous;resumed=0;}
    void Pause(uint64_t now,unsigned routeMs){Commit(now,routeMs);resumed=0;}
    void Resume(uint64_t now){resumed=label?now:0;}
};
}
#endif
