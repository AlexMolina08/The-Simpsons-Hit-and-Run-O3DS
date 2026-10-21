#ifndef RADSOUND_CTR_LID_GREETING_STATE_HPP
#define RADSOUND_CTR_LID_GREETING_STATE_HPP

// The APT callback only records a confirmed sleep/wake pair. File and DSP work
// runs later on the game's service thread, after libctru has restored audio.
namespace ctrLidGreeting {
class State {
public:
    enum { ClipCount = 10 };
    State() : flags(0), random(1), cursor(ClipCount), last(-1) {}
    void Seed(unsigned value) { random=value ? value : 1; cursor=ClipCount; last=-1; }
    void Sleep() { __atomic_store_n(&flags,1u,__ATOMIC_RELEASE); }
    void Wake() {
        unsigned sleeping=1;
        __atomic_compare_exchange_n(&flags,&sleeping,2u,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    }
    bool Consume() {
        unsigned pending=2;
        return __atomic_compare_exchange_n(&flags,&pending,0u,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    }
    void Clear() { __atomic_store_n(&flags,0u,__ATOMIC_RELEASE); }
    unsigned Next() {
        if(cursor==ClipCount) {
            for(unsigned i=0;i<ClipCount;++i) bag[i]=i;
            for(unsigned i=ClipCount-1;i>0;--i) {
                unsigned j=Random()%(i+1), value=bag[i];bag[i]=bag[j];bag[j]=value;
            }
            if(int(bag[0])==last) { unsigned value=bag[0];bag[0]=bag[1];bag[1]=value; }
            cursor=0;
        }
        last=bag[cursor++];return unsigned(last);
    }
private:
    unsigned Random() { random^=random<<13;random^=random>>17;random^=random<<5;return random; }
    unsigned flags,random,cursor,bag[ClipCount];
    int last;
};
}
#endif
