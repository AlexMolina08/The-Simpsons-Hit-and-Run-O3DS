#ifndef SRR_WORLD_STREAMING_H
#define SRR_WORLD_STREAMING_H

// Diagnostic candidates remain separate from the accepted release policy.
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <cstdio>
class StaticEntityDSG;
class Avatar;
namespace WorldStreaming {
extern bool predictive, audit, manual, capture, passive, passiveActive;
// Private stress traversal moves streaming focus and camera without collisions.
// Its timings are not a gameplay performance comparison.
extern bool fly, flightActive, idleLoader;
extern unsigned incidentRoute, flightCpuUs;
extern float flightFov;
extern bool worldPriority, worldRetention, pedestrianReuse;
extern Avatar* flightAvatar;
extern float flightPosition[3], flightVelocity[3], flightSpeed;
extern unsigned pedestrianLimit, traversal, cameraSweep;
extern float cruiseSpeed;
bool ParseArgument(const char*);
unsigned Key(const char*);
void Event(const char* stage, const char* zone);
void EventUID(const char* stage,unsigned long long zone);
void Init();
void MarkerImage();
void RequestEvent(const char* stage,const char* filename,const char* section);
void QueuedFile(const char* filename,unsigned position,unsigned total,bool active,bool promoted);
void BeginFrame(unsigned routeMs,float distance);
void EndFrame();
void Loaded(unsigned long long zone, StaticEntityDSG*);
void Unloaded(unsigned long long zone);
void ResetResidents();
void Displayed(StaticEntityDSG*, unsigned draws);
void Dump(FILE*,const char* trial);
bool ScanDue(unsigned now);
float LeadSeconds(float speed);
void ObserveLoad(unsigned milliseconds);
bool Admit(unsigned sections, unsigned capacity, unsigned normalFree, unsigned linearFree);
enum PedSpawnStage { PedRequest,PedLimit,PedDisabled,PedNoFree,PedNoPath,PedNoCrossing,PedRejected,PedSpawned,PedReadyReuse };
void PedSpawn(PedSpawnStage);
void PedSlots(unsigned reserved,unsigned awaitingActivation);
}
#endif
#endif
