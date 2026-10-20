#ifndef OLD3DS_POOLS_H
#define OLD3DS_POOLS_H

// Diagnostic-only, disjoint wall-clock segments. Exactly one bucket runs on
// main; changing buckets closes the preceding segment. No nested totals exist.
// Loader-thread scopes are ignored rather than racing main's capture state.
#ifdef SRR2_OLD3DS_COARSE_POOLS
#include <3ds.h>
#include <cstdio>
namespace Old3dsPools {
enum Family { None, Physics, Presentation, Recipe, Streaming };
enum Stage { Uncovered, Areas, Ground, PreSim, VehiclePrep, CharacterPrep,
    Actions, Detect, Solve, Vehicles, Characters, Camera, Dynamics,
    World, Hud, Bottom, Upstream, Backend, Pose, Fallback,
    LoadHandoff, Callbacks, Count };
extern unsigned family;
extern unsigned long long ticks[Count];
extern unsigned calls[Count];
void Configure(unsigned);
void Begin(unsigned);
void End();
void Dump(FILE*);
bool InspectRecipe();
int Select(unsigned, unsigned);
void Restore(int);
struct Scope {
    int previous;
    Scope(unsigned group, unsigned stage):previous(Select(group, stage)) {}
    ~Scope() { Restore(previous); }
};
}
#define O3_POOL_JOIN_(a,b) a##b
#define O3_POOL_JOIN(a,b) O3_POOL_JOIN_(a,b)
#define O3_POOL(group,stage) Old3dsPools::Scope O3_POOL_JOIN(poolScope,__LINE__)(Old3dsPools::group,Old3dsPools::stage)
#else
#define O3_POOL(group,stage) ((void)0)
#endif
#endif
