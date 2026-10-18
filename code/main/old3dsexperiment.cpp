#include <main/old3dsexperiment.h>
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
#include <main/old3dsdiag.h>
#include <citro3d.h>
#include <cstdio>
#include <cstring>
unsigned g_o3Mode=O3_REFERENCE, g_o3WorldDepth=0;
bool g_o3Detail=true;
namespace {
const unsigned Capacity=2048, Window=90;
struct Frame {
    u64 start, work, period;
    u32 inclusive[O3E_SCOPE_COUNT], exclusive[O3E_SCOPE_COUNT];
    u32 step[10], phase[4];
    unsigned fixed;
    unsigned id, mode, route, ordinal, elapsed, draws, vertices, suppressed, mip, noMip;
    unsigned entities, substeps, pairs, areas, linear;
};
struct Completion { u64 swap, visible; float gpu; unsigned id; };
Frame frames[Capacity], current;
Completion completions[Capacity];
unsigned count=0, lost=0, run=0, ordinal=0, submittedId=0;
unsigned startEntities=0, startSubsteps=0, startPairs=0, startAreas=0;
bool armed=false, fixedElapsed=false, recording=false;
bool external=false, externalCapture=false;
unsigned externalMode=O3_REFERENCE;
u64 lastStart=0;
// Stack totals let scopes report both inclusive and exclusive ticks without
// adding nested work. All detailed scopes are synchronous on the game thread.
u64 children[32]; unsigned depth=0;
ThreadFunc vblankCallback=nullptr; void* vblankData=nullptr;
unsigned pendingVisible=0;
void onVblank(void*) {
    if(vblankCallback) vblankCallback(vblankData);
    const unsigned pending=__atomic_exchange_n(&pendingVisible,0,__ATOMIC_ACQ_REL);
    if(pending) {
        Completion& c=completions[pending%Capacity];
        if(c.id==pending) c.visible=svcGetSystemTick();
    }
}
unsigned entities() { unsigned n=0; for(unsigned i=0;i<O3_CATEGORY_COUNT;++i)n+=g_old3dsEntities[i].calls;return n; }
double ms(u64 ticks) { return ticks*1000.0/SYSCLOCK_ARM11; }
}
// Preserve Citro3D's VBlank callback and framebuffer swap. Observe presentation
// at the following top-screen VBlank, not at C3D_FrameEnd or CPU-loop completion.
extern "C" void __real_gspSetEventCallback(GSPGPU_Event,ThreadFunc,void*,bool);
extern "C" void __wrap_gspSetEventCallback(GSPGPU_Event event,ThreadFunc cb,void* data,bool once) {
    if(event==GSPGPU_EVENT_VBlank0 && cb && !once) {
        vblankCallback=cb;vblankData=data;
        __real_gspSetEventCallback(event,onVblank,nullptr,false);
    } else __real_gspSetEventCallback(event,cb,data,once);
}
extern "C" void __real_gfxScreenSwapBuffers(gfxScreen_t,bool);
extern "C" void __wrap_gfxScreenSwapBuffers(gfxScreen_t screen,bool stereo) {
    __real_gfxScreenSwapBuffers(screen,stereo);
    const unsigned id=__atomic_load_n(&submittedId,__ATOMIC_ACQUIRE);
    if(screen==GFX_TOP && id) {
        Completion& c=completions[id%Capacity];
        c.id=id;c.swap=svcGetSystemTick();c.gpu=C3D_GetDrawingTime();c.visible=0;
        __atomic_store_n(&pendingVisible,id,__ATOMIC_RELEASE);
    }
}
O3ExperimentTimer::O3ExperimentTimer(unsigned s):slot(s),begin(0),childBegin(0) {
    if(!recording || !g_o3Detail || depth>=32) return;
    childBegin=children[depth];children[++depth]=0;begin=svcGetSystemTick();
}
O3ExperimentTimer::~O3ExperimentTimer() {
    if(!begin) return;
    const u64 elapsed=svcGetSystemTick()-begin, child=children[depth--];
    if(slot==O3E_SUBSTEP) {
        for(unsigned i=0;i<10;++i) if(!current.step[i]) {current.step[i]=u32(elapsed);break;}
    }
    current.inclusive[slot]+=u32(elapsed);
    current.exclusive[slot]+=u32(elapsed>=child?elapsed-child:0);
    children[depth]=childBegin+elapsed;
}
void O3ExperimentExternal(unsigned mode,bool capture) {
    external=true;externalMode=mode<=O3_SUPPRESS?mode:O3_REFERENCE;
    externalCapture=capture;armed=false;fixedElapsed=false;
}
void O3ExperimentExternalCapture(bool capture) { externalCapture=capture; }
void O3ExperimentArm(bool fixed) {
    if(external) return;
    fixedElapsed=fixed;ordinal=0;++run;armed=true;lastStart=0;
}
bool O3ExperimentFixed() { return armed && fixedElapsed; }
void O3ExperimentBegin(unsigned frame,bool gameplay) {
    static unsigned warmup=0;
    if(!external && gameplay && !run && ++warmup==60) O3ExperimentArm(false);
    recording=gameplay && (external?externalCapture:(armed && ordinal<9*Window));
    if(!recording) {g_o3Mode=O3_REFERENCE;g_o3Detail=true;return;}
    static const unsigned modes[9]={O3_REFERENCE,O3_LOW,O3_REFERENCE,O3_COVERAGE,
        O3_REFERENCE,O3_MIP,O3_REFERENCE,O3_SUPPRESS,O3_REFERENCE};
    memset(&current,0,sizeof(current));
    current.id=frame+1;current.mode=g_o3Mode=external?externalMode:modes[ordinal/Window];
    current.fixed=fixedElapsed;current.route=run;current.ordinal=ordinal;current.start=svcGetSystemTick();
    if(lastStart) current.period=current.start-lastStart;
    lastStart=current.start;g_o3Detail=g_o3Mode!=O3_LOW;
    current.linear=linearSpaceFree();
    startEntities=entities();startSubsteps=g_old3dsPhysicsSubsteps;
    startPairs=g_old3dsCollisionPairs;startAreas=g_old3dsPhysicsAreas;
}
void O3ExperimentPhase(unsigned phase,u64 ticks) {
    if(recording && phase<4) current.phase[phase]=u32(ticks);
}
void O3ExperimentEnd(u64 ticks,unsigned elapsed) {
    if(!recording) return;
    current.work=ticks;current.elapsed=elapsed;
    current.entities=entities()-startEntities;current.substeps=g_old3dsPhysicsSubsteps-startSubsteps;
    current.pairs=g_old3dsCollisionPairs-startPairs;current.areas=g_old3dsPhysicsAreas-startAreas;
    if(count<Capacity) frames[count++]=current; else ++lost;
    if(++ordinal==9*Window && !external) armed=false;
    recording=false;
}
void O3ExperimentSubmit() {
    __atomic_store_n(&submittedId,recording && count<Capacity ? count+1 : 0,__ATOMIC_RELEASE);
}
void O3ExperimentDraw(unsigned vertices) {
    if(!recording || !g_o3WorldDepth) return;
    ++current.draws;current.vertices+=vertices;
    if(g_o3Mode==O3_SUPPRESS) ++current.suppressed;
}
void O3ExperimentMip(bool available) {
    if(recording && g_o3WorldDepth) ++(available?current.mip:current.noMip);
}
void O3ExperimentDump() {
    printf("O3_CAUSAL version=1 frames=%u lost=%u bytes=%u window=%u pmu=unsupported_user_api presentation=next_vblank_after_swap gpu=completed_submitted_frame scopes=inclusive_and_exclusive low=sampled_scopes_uid_disabled_cache_guards_retained\n",
        count,lost,unsigned(sizeof(frames)+sizeof(completions)),Window);
    for(unsigned i=0;i<count;++i) {
        const Frame& f=frames[i];const Completion& c=completions[(i+1)%Capacity];
        const bool valid=c.id==i+1;
        printf("O3_FRAME id=%u route=%u ordinal=%u mode=%u fixed=%u elapsed=%u work=%.4f period=%.4f draws=%u vertices=%u suppressed=%u mip=%u nomip=%u entities=%u substeps=%u pairs=%u areas=%u linear=%u swap_tick=%llu visible_tick=%llu gpu_ms=%.4f",
            f.id,f.route,f.ordinal,f.mode,f.fixed,f.elapsed,ms(f.work),ms(f.period),f.draws,f.vertices,f.suppressed,f.mip,f.noMip,f.entities,f.substeps,f.pairs,f.areas,f.linear,
            valid?c.swap:0,valid?c.visible:0,valid?c.gpu:-1.0);
        for(unsigned j=0;j<O3E_SCOPE_COUNT;++j)printf(" i%u=%.4f x%u=%.4f",j,ms(f.inclusive[j]),j,ms(f.exclusive[j]));
        for(unsigned j=0;j<4;++j) printf(" phase%u=%.4f",j,ms(f.phase[j]));
        for(unsigned j=0;j<10;++j) printf(" step%u=%.4f",j,ms(f.step[j]));
        printf("\n");
    }
    if(fflush(stdout)==0 && !ferror(stdout)) {count=lost=0;}
}
#endif
