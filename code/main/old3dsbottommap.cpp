#include <main/old3dsbottommap.h>
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
#include <3ds.h>
namespace Old3dsBottomMap {
bool enabled=true;
static Old3dsBottomMapDeadline deadline;
static uint64_t pendingTick=0;
static unsigned drawn=0,skipped=0;
void Reset(){deadline.Reset();}
bool Due(){
    if(!enabled){deadline.Reset();return true;}
    pendingTick=svcGetSystemTick();
    if(deadline.Due(pendingTick))return true;
    ++skipped;return false;
}
void Rendered(){
    ++drawn;
    if(enabled)deadline.Rendered(pendingTick,SYSCLOCK_ARM11/15);
}
unsigned DrawCount(){return drawn;}
void Dump(FILE* file){
    fprintf(file,"NATIVE_BOTTOM_MAP enabled=%u requested_hz=15 drawn=%u skipped=%u counts_are_render_attempts=1 simulation_updates_unchanged=1\n",enabled,drawn,skipped);
}
}
#endif
