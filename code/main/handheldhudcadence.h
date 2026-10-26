#ifndef HANDHELD_HUD_CADENCE_H
#define HANDHELD_HUD_CADENCE_H
#include <stdint.h>

// Presentation-only absolute deadline. Missed slots never accumulate work.
class HandheldHudDeadline {
    uint64_t next;
public:
    HandheldHudDeadline():next(0) {}
    void Reset(){next=0;}
    bool Due(uint64_t now) const{return !next || now>=next;}
    void Rendered(uint64_t now,uint64_t period){
        if(!next)next=now+period;
        else next+=(1+(now-next)/period)*period;
    }
};
class HandheldHudAdaptive {
    unsigned filtered=0,busy=0,cool=0,hz=15,minimum=10;
public:
    void Reset(){filtered=busy=cool=0;hz=15;}
    unsigned Hz()const{return hz;}
    void SetMinimum(unsigned value){
        if(value!=8 && value!=10)return;
        minimum=value;if(hz<15)hz=minimum;
    }
    void Observe(unsigned workUs){
        if(workUs>250000)workUs=250000;
        filtered=filtered?(filtered*7+workUs)/8:workUs;
        if(hz==15){
            cool=0;busy=filtered>36000?busy+1:0;
            if(busy>=8){hz=minimum;busy=0;}
        }else{
            busy=0;cool=filtered<33000?cool+1:0;
            if(cool>=60){hz=15;cool=0;}
        }
    }
};
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
#include <cstdio>
namespace HandheldHudCadence {
extern bool enabled;
// Zero preserves 15 Hz; one tests 10 Hz; two selects the configured minimum
// or 15 Hz from CPU load. The private minimum comparison supports 8 and 10 Hz.
extern unsigned cadenceMode;
extern unsigned minimumHz;
void ObserveFrame(unsigned workUs,bool gameplay);
void ForceUpdate();
void SetUrgent(bool);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
extern bool boostPickupAnimations;
#endif
void Reset();
bool Due();
void Rendered();
unsigned DrawCount();
void Dump(FILE*);
}
#endif
#endif
