#ifndef OLD3DS_VEHICLE_PROFILE_H
#define OLD3DS_VEHICLE_PROFILE_H
#include <3ds.h>
#include <main/old3dssampling.h>
#include <cstdio>
namespace Old3dsVehicleProfile {
enum Phase { StateStore, LocomotionPre, LocomotionUpdate, LocomotionPost,
             BasisWheels, Events, Pose, DoorsJoint, Collision, SceneMove, Remainder, Count };
struct Counters {u64 ticks[2][Count];unsigned calls[2],samples[2];};
extern Counters counters;
inline bool& CaptureEnabled(){static bool enabled=true;return enabled;}
class Sample {
    unsigned mode;bool enabled;u64 previous;
public:
    explicit Sample(bool physics):mode(physics?1:0),enabled(CaptureEnabled() && Old3dsRotatingSample(counters.calls[mode]++,5)),previous(enabled?svcGetSystemTick():0) {}
    void End(Phase phase) {
        if(enabled) {const u64 now=svcGetSystemTick();counters.ticks[mode][phase]+=now-previous;previous=now;}
    }
    ~Sample(){if(enabled){End(Remainder);++counters.samples[mode];}}
};
enum ExternalPhase { DynamicObjects, AnimCollisions, ExternalCount };
struct ExternalCounters {u64 ticks[ExternalCount];unsigned calls,samples;};
extern ExternalCounters externalCounters;
class ExternalSample {
    bool enabled;u64 previous;
public:
    ExternalSample():enabled(CaptureEnabled() && Old3dsRotatingSample(externalCounters.calls++,5)),
        previous(enabled?svcGetSystemTick():0) {}
    void End(ExternalPhase phase) {
        if(enabled) {const u64 now=svcGetSystemTick();externalCounters.ticks[phase]+=now-previous;previous=now;}
    }
    ~ExternalSample(){if(enabled)++externalCounters.samples;}
};
inline void Dump(FILE* file=stdout)
{
    const char* names[]={"state_store","locomotion_pre","locomotion_update","locomotion_post",
                         "basis_wheels","events","pose","doors_joint","collision","scene_move","remainder"};
    for(unsigned mode=0;mode<2;++mode) for(unsigned phase=0;phase<Count;++phase)
        std::fprintf(file,"OLD3DS_VEHICLE_PHASE mode=%s name=%s calls=%u samples=%u sampled_ms=%.6f\n",
            mode?"physics":"traffic",names[phase],counters.calls[mode],counters.samples[mode],
            counters.ticks[mode][phase]*1000.0/SYSCLOCK_ARM11);
    const char* externalNames[]={"dynamic_objects","anim_collisions"};
    for(unsigned phase=0;phase<ExternalCount;++phase)
        std::fprintf(file,"OLD3DS_VEHICLE_EXTERNAL name=%s calls=%u samples=%u sampled_ms=%.6f\n",
            externalNames[phase],externalCounters.calls,externalCounters.samples,
            externalCounters.ticks[phase]*1000.0/SYSCLOCK_ARM11);
}
}
#endif
