#ifndef SRR_OLD3DS_RECURRING_H
#define SRR_OLD3DS_RECURRING_H
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <3ds.h>
#include <cstdio>
class IEntityDSG;
namespace Old3dsRecurring {
enum Stage { Update,Render,Gameplay,Avatar,Footprint,Garbage,Effects,Physics,Pedestrian,Trigger,Traffic,Actor,Interior,Skidmark,Coin,HitRun,Presentation,Playing,World,Visibility,List,Opaque,Translucent,Shadow,Backend,Material,Submit,Uniforms,PhysicsAreas,PhysicsGround,PhysicsPreSim,PhysicsVehiclePrep,PhysicsCharacterPrep,PhysicsActions,PhysicsDetect,PhysicsSolve,PhysicsVehicles,PhysicsCharacters,PhysicsCamera,PhysicsDynamics,Count };
extern bool active,requested;
extern unsigned stationaryMs;
struct Scope {
    Scope* parent=nullptr;u64 start=0,children=0;unsigned stage;
    explicit Scope(unsigned s):stage(s){if(active)Enter();}
    ~Scope(){if(start)Leave();}
    void Enter();void Leave();
};
struct Entity {
    u64 start=0,backend=0;unsigned slot=0,draws=0,vertices=0;
    Entity(IEntityDSG*,unsigned);~Entity();
};
void Begin(unsigned,unsigned,bool);void End();void Dump(FILE*);
}
#define O3_RECUR_JOIN_(a,b) a##b
#define O3_RECUR_JOIN(a,b) O3_RECUR_JOIN_(a,b)
#ifdef SRR2_OLD3DS_RECURRING_PROFILE
#define O3_RECUR(s) Old3dsRecurring::Scope O3_RECUR_JOIN(recurringScope,__LINE__)(Old3dsRecurring::s)
#define O3_RECUR_ENTITY(e,p) Old3dsRecurring::Entity O3_RECUR_JOIN(recurringEntity,__LINE__)(e,p)
#else
#define O3_RECUR(s) ((void)0)
#define O3_RECUR_ENTITY(e,p) ((void)0)
#endif
#else
#define O3_RECUR(s) ((void)0)
#define O3_RECUR_ENTITY(e,p) ((void)0)
#endif
#endif
