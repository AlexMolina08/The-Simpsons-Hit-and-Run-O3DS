#include <main/old3dscadence.h>
#if defined(SRR2_OLD3DS_NATIVE_BENCH) && !defined(SRR2_OLD3DS_CAUSAL_DIAG)
#include <3ds.h>
#include <main/nativebench.h>
namespace Old3dsCadence {
bool enabled=false;
namespace {
const unsigned Capacity=2048;
struct Observation {u64 ready,observed;unsigned identity;};
Observation observations[Capacity];
ThreadFunc callback=nullptr;
void* callbackData=nullptr;
unsigned submitted=0,pending=0,replaced=0,first=0,last=0;
void Vblank(void*)
{
    if(callback)callback(callbackData);
    // A queue completion only requests presentation. Wait for GSP to consume
    // the top-screen request before publishing a VBlank observation.
    if(!pending || gspIsPresentPending(GSP_SCREEN_TOP))return;
    Observation& row=observations[pending%Capacity];
    row.observed=svcGetSystemTick();
    __atomic_store_n(&row.identity,pending,__ATOMIC_RELEASE);
    pending=0;
}
}
void Submit(unsigned identity){__atomic_store_n(&submitted,enabled && NativeBench::collecting?identity:0,__ATOMIC_RELEASE);}
void Swapped()
{
    const unsigned identity=__atomic_load_n(&submitted,__ATOMIC_ACQUIRE);
    if(!identity)return;
    if(pending)++replaced;
    pending=identity;
    if(!first)first=identity;
    last=identity;
    Observation& row=observations[identity%Capacity];
    __atomic_store_n(&row.identity,0u,__ATOMIC_RELEASE);
    row.ready=svcGetSystemTick();row.observed=0;
}
void SetCallback(ThreadFunc cb,void* data){callback=cb;callbackData=data;}
void Dump(FILE* file)
{
    if(!enabled)return;
    // Capture has ended. Retire the final swap before reading GSP-owned rows.
    // This wait occurs outside the measured route and adds no pacing to it.
    gspWaitForVBlank();gspWaitForVBlank();
    fprintf(file,"NATIVE_CADENCE enabled=1 observation=gsp_consumed_at_vblank_irq scanout_timestamp=0 irq_dispatch_uncertainty=1 replaced=%u first=%u last=%u capacity=%u\n",replaced,first,last,Capacity);
    u64 previous=0;
    for(unsigned id=first;id && id<=last;++id){
        const Observation& row=observations[id%Capacity];
        const bool valid=__atomic_load_n(&row.identity,__ATOMIC_ACQUIRE)==id;
        const u64 tick=valid?row.observed:0;
        fprintf(file,"NATIVE_PRESENT submit_id=%u observed=%u ready_tick=%llu vblank_tick=%llu interval_us=%.3f\n",id,valid,valid?row.ready:0,tick,
            tick && previous?(tick-previous)*1000000.0/SYSCLOCK_ARM11:0.0);
        if(tick)previous=tick;
    }
}
}
extern "C" void __real_gspSetEventCallback(GSPGPU_Event,ThreadFunc,void*,bool);
extern "C" void __wrap_gspSetEventCallback(GSPGPU_Event event,ThreadFunc cb,void* data,bool once)
{
    if(event==GSPGPU_EVENT_VBlank0 && cb && !once){
        Old3dsCadence::SetCallback(cb,data);
        __real_gspSetEventCallback(event,Old3dsCadence::Vblank,nullptr,false);
    }else __real_gspSetEventCallback(event,cb,data,once);
}
extern "C" void __real_gfxScreenSwapBuffers(gfxScreen_t,bool);
extern "C" void __wrap_gfxScreenSwapBuffers(gfxScreen_t screen,bool stereo)
{
    __real_gfxScreenSwapBuffers(screen,stereo);
    if(screen==GFX_TOP)Old3dsCadence::Swapped();
}
#endif
