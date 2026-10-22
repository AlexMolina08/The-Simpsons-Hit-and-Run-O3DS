#ifndef OLD3DS_PERFORMANCE_H
#define OLD3DS_PERFORMANCE_H
#include <main/graphics3ds.h>

// Old 3DS quality policy. Mission-owned entities and simulation remain active.
// Mode zero reproduces the reference workload for same-binary hardware tests.
namespace Old3dsPerformance {
#ifdef SRR2_OLD_3DS
extern unsigned mode;
inline bool Enabled() { return mode != 0; }
inline float ParticleEmissionScale() { return mode >= 5 ? 0.5f : 1.0f; }
inline bool DistantPedestrianActions() { return mode >= 6; }
inline bool FlatLighting() { return mode >= 7 && !Graphics3DS::ImprovedLighting(); }
inline bool ReducedAudioSearch() { return mode >= 3; }
inline float DrawDistance() { return mode >= 4 ? 120.0f : mode >= 2 ? 70.0f : 100.0f; }
inline bool AggressiveSmallCull() { return mode != 0 && mode < 4; }
inline unsigned PhysicsStepMilliseconds() { return mode >= 5 ? 50u : mode >= 2 ? 40u : 35u; }
inline unsigned TrafficLimit() { return mode >= 2 ? 1u : 2u; }
inline unsigned PedestrianLimit() { return mode >= 4 ? 4u : mode >= 2 ? 1u : 3u; }
#else
inline unsigned PhysicsStepMilliseconds() { return 35u; }
inline bool Enabled() { return false; }
inline bool FlatLighting() { return false; }
inline bool DistantPedestrianActions() { return false; }
inline float ParticleEmissionScale() { return 1.0f; }
inline bool ReducedAudioSearch() { return false; }
inline float DrawDistance() { return 200.0f; }
inline bool AggressiveSmallCull() { return false; }
inline unsigned TrafficLimit() { return 5u; }
inline unsigned PedestrianLimit() { return 7u; }
#endif
}
#endif
