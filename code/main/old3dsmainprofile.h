#ifndef OLD3DS_MAIN_PROFILE_H
#define OLD3DS_MAIN_PROFILE_H
#ifdef SRR2_OLD3DS_MAIN_PROFILE
#include <3ds.h>
#include <cstdio>
namespace Old3dsMainProfile {
enum Stage { GameUpdate,WorldLayer,WorldScene,Opaque,Translucent,Geometry,Composite,PolySkin,
    PoseEvaluate,PoseJoint,MatrixMultiply,DrawPrim,ApplyMaterial,MaterialProtocol,MaterialPass,
    MaterialCapture,MaterialPreRender,UniformCapture,LightRebuild,PrimDisplay,GeometryCapture,
    BeginPrepared,FlushPrepared,FrameBegin,Viewport,VehicleUpdate,StoreJoints,PhysicsUpdate,
    PhysicsSubstep,CharactersUpdate,PoserAssign,PoserFill,
    HudInterface,HudRadar,HudSprite,HudFont,HudWrap,HudArt,Count };
extern void* mainTls;
extern bool enabled;
struct Scope {
    Scope* parent=nullptr;const void* caller=nullptr;u64 start=0,children=0;unsigned stage,directChildren=0,descendants=0;
    explicit Scope(unsigned value,const void* site=nullptr):caller(site),stage(value)
    {
        // Thread identity is checked before touching main's mutable frame state.
        // The supported libctru TLS read does not suspend or inspect another thread.
        if(getThreadLocalStorage()==__atomic_load_n(&mainTls,__ATOMIC_RELAXED) && __atomic_load_n(&enabled,__ATOMIC_RELAXED))Enter();
    }
    ~Scope(){if(start)Leave();}
    void Enter();void Leave();
};
void Configure(bool);void FocusDropWindow(bool);void BeginFrame(unsigned,bool,unsigned routeMs=0);void EndFrame(u64);void Dump(FILE*,double);
}
#define O3_MP_JOIN_(a,b) a##b
#define O3_MP_JOIN(a,b) O3_MP_JOIN_(a,b)
#define O3_MAIN_SCOPE(stage) Old3dsMainProfile::Scope O3_MP_JOIN(mainProfileScope,__LINE__)(Old3dsMainProfile::stage,__builtin_return_address(0))
#else
#define O3_MAIN_SCOPE(stage) ((void)0)
#endif
#endif
