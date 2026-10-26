#include <main/handheldhudcadence.h>
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
#include <3ds.h>
#include <main/graphics3ds.h>
namespace HandheldHudCadence {
bool enabled=true;
unsigned cadenceMode=0;
unsigned minimumHz=10;
static HandheldHudAdaptive adaptive;
static bool urgent=false;
static unsigned forced=0,drawn8=0,drawn10=0,drawn15=0;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
bool directHudBackground=false;
bool boostPickupAnimations=true;
unsigned hudBackgroundLookups=0,hudBackgroundDirect=0;
#endif
static HandheldHudDeadline deadline;
static uint64_t pendingTick=0;
static unsigned drawn=0,skipped=0;
void Reset(){deadline.Reset();}
void ForceUpdate(){deadline.Reset();++forced;}
void SetUrgent(bool value){urgent=value;}
void ObserveFrame(unsigned workUs,bool gameplay){
    if(cadenceMode!=2)return;
    adaptive.SetMinimum(minimumHz);
    if(gameplay)adaptive.Observe(workUs);else adaptive.Reset();
}
static unsigned Rate(){
    bool animationBoost=urgent;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    animationBoost=animationBoost && boostPickupAnimations;
#endif
    if(!cadenceMode || Graphics3DS::IsNew() || animationBoost)return 15;
    return cadenceMode==1?10:adaptive.Hz();
}
bool Due(){
    if(!enabled){deadline.Reset();return true;}
    pendingTick=svcGetSystemTick();
    if(deadline.Due(pendingTick))return true;
    ++skipped;return false;
}
void Rendered(){
    ++drawn;
    const unsigned hz=Rate();
    if(hz==8)++drawn8;else if(hz==10)++drawn10;else ++drawn15;
    if(enabled)deadline.Rendered(pendingTick,SYSCLOCK_ARM11/hz);
}
unsigned DrawCount(){return drawn;}
void Dump(FILE* file){
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    fprintf(file,"NATIVE_HUD_BACKGROUND direct=%u lookups=%u direct_paths=%u persistent_cache=0 whole_session=1\n",directHudBackground,hudBackgroundLookups,hudBackgroundDirect);
    fprintf(file,"NATIVE_HUD_ANIMATION boost_pickups=%u new_model_keeps_15hz=1 immediate_priority_updates=1\n",boostPickupAnimations);
#endif
    fprintf(file,"NATIVE_BOTTOM_MAP enabled=%u cadence_mode=%u minimum_hz=%u requested_hz=%u drawn=%u skipped=%u drawn8=%u drawn10=%u drawn15=%u forced=%u counts_are_render_attempts=1 simulation_updates_unchanged=1\n",enabled,cadenceMode,minimumHz,Rate(),drawn,skipped,drawn8,drawn10,drawn15,forced);
}
}
#endif
