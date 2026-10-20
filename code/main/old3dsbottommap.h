#ifndef OLD3DS_BOTTOM_MAP_H
#define OLD3DS_BOTTOM_MAP_H
#include <stdint.h>

// Presentation-only absolute deadline. Missed slots never accumulate work.
class Old3dsBottomMapDeadline {
    uint64_t next;
public:
    Old3dsBottomMapDeadline():next(0) {}
    void Reset(){next=0;}
    bool Due(uint64_t now) const{return !next || now>=next;}
    void Rendered(uint64_t now,uint64_t period){
        if(!next)next=now+period;
        else next+=(1+(now-next)/period)*period;
    }
};
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
#include <cstdio>
namespace Old3dsBottomMap {
extern bool enabled;
void Reset();
bool Due();
void Rendered();
unsigned DrawCount();
void Dump(FILE*);
}
#endif
#endif
