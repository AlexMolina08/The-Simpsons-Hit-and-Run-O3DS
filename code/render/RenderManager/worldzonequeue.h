#ifndef SRR_WORLD_ZONE_QUEUE_H
#define SRR_WORLD_ZONE_QUEUE_H
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <climits>
#include <limits>

// Only the renderer's pending zone events use this queue. Locators remain alive
// through deferred callbacks, and repeated adjacent demand stays idempotent.
// Distinct A/B/A demand preserves its complete FIFO order.
template<class Locator, unsigned InlineCapacity=32> class WorldZoneQueue {
public:
    static_assert(InlineCapacity>0,"Zone queue requires inline storage");
    unsigned mSize=InlineCapacity,mUseSize=0;
    Locator** mpData=inlineData;
    unsigned peak=0,duplicates=0,growths=0,failures=0;

    WorldZoneQueue() = default;
    ~WorldZoneQueue(){ClearUse();if(active)active->Release();if(mpData!=inlineData)::free(mpData);}
    WorldZoneQueue(const WorldZoneQueue&)=delete;
    WorldZoneQueue& operator=(const WorldZoneQueue&)=delete;

    bool Allocate(unsigned capacity){return Reserve(capacity);}
    bool Add(Locator* locator,bool activeDemand=true){
        if(!locator){++failures;return false;}
        if((mUseSize && mpData[mUseSize-1]==locator) || (!mUseSize && activeDemand && active==locator)){
            ++duplicates;return true;
        }
        if(mUseSize==mSize){
            if(mSize>UINT_MAX/2){++failures;return false;}
            if(!Reserve(mSize*2))return false;
        }
        locator->AddRef();mpData[mUseSize++]=locator;
        if(mUseSize>peak)peak=mUseSize;
        return true;
    }
    void SetActive(Locator* locator){
        if(locator==active)return;
        if(locator)locator->AddRef();
        Locator* previous=active;active=locator;
        if(previous)previous->Release();
    }
    Locator* TakeNext(){
        if(!mUseSize)return nullptr;
        Locator* next=mpData[0];
        --mUseSize;
        if(mUseSize)std::memmove(mpData,mpData+1,mUseSize*sizeof(*mpData));
        // Transfer the queued reference to the active callback owner.
        Locator* previous=active;active=next;
        if(previous)previous->Release();
        return next;
    }
    void ClearUse(){
        // Keep the active callback's locator until its replacement starts.
        // A level dump may cancel pending demand while that callback is live.
        while(mUseSize){Locator* value=mpData[--mUseSize];value->Release();}
    }
    bool Failed() const{return failures!=0;}
private:
    Locator* inlineData[InlineCapacity]={};
    Locator* active=nullptr;
    bool Reserve(unsigned capacity){
        if(capacity<=mSize)return true;
        if(capacity>std::numeric_limits<std::size_t>::max()/sizeof(*mpData)){++failures;return false;}
        auto* replacement=static_cast<Locator**>(::malloc(std::size_t(capacity)*sizeof(*mpData)));
        if(!replacement){++failures;return false;}
        if(mUseSize)std::memcpy(replacement,mpData,mUseSize*sizeof(*mpData));
        if(mpData!=inlineData)::free(mpData);
        mpData=replacement;mSize=capacity;++growths;return true;
    }
};
#endif
