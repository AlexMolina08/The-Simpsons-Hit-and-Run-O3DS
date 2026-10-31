#ifndef SRR_WORLD_RESIDENCY_H
#define SRR_WORLD_RESIDENCY_H
#if defined(SRR2_OLD3DS_NATIVE_BENCH) || defined(SRR2_OLD3DS_WORLD_RESIDENCY)
#include <cstdio>
class StaticEntityDSG;
class ZoneEventLocator;
namespace WorldResidency {
// No engine pointers survive section destruction. Prepared levels are admitted
// as a complete asset set before their first exterior file is opened.
void Init();
unsigned GroundCapacity();
void Gameplay(unsigned level);
void InitialDemand(const char* filename);
void Trigger(ZoneEventLocator*, unsigned player);
void Bind(unsigned long long section, StaticEntityDSG*);
bool Visible(StaticEntityDSG*);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
void CacheLookup(bool enabled);
unsigned CachePolicy();
#endif
void Unloaded(unsigned long long section);
void Reset();
void ClearDemand();
struct LevelStatus { unsigned expected,bound,bad,wanted,loads,unloads; };
LevelStatus Status(unsigned level);
void Dump(FILE*);
void PlacementFailure(bool ground);
bool Failed();
}
#endif
#endif
