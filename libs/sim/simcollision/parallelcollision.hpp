#ifndef OLD3DS_PARALLEL_COLLISION_HPP
#define OLD3DS_PARALLEL_COLLISION_HPP
#include "simcollision/collisiondetector.hpp"
#include "simcollision/collision.hpp"
#include "simcollision/parallelcollisionmetrics.h"
namespace sim {
class ParallelCollisionState {
public:
    static constexpr unsigned MaxAreas=64,MaxVolumes=1024;
    struct PairSnapshot {int contacts;bool needsUpdate;float safeTime;};
    struct Job {
        TArray<CollisionObjectPair>* pairs=nullptr;
        TArray<Collision>* contacts=nullptr;
        TArray<Collision> candidate;
        TArray<PairSnapshot> candidateFlags;
        unsigned tail=0;
        bool overflow=false,updatedPair=false;
    } jobs[MaxAreas];
    struct VolumeSnapshot {CollisionVolume* volume;float values[24];unsigned count;bool updated;};
    VolumeSnapshot volumes[MaxVolumes];unsigned volumeCount=0;
    CollisionVolume* safeVolumes[4096]={};unsigned safeStamps[4096]={},safeGeneration=0;
    bool SupportedVolume(CollisionVolume* volume,CollisionObject* owner,unsigned depth=0);
    struct Root {CollisionVolume* volume;unsigned next;};
    struct RootGroup {void* key;unsigned first,last;};
    Root roots[MaxVolumes];RootGroup groups[MaxVolumes];
    unsigned rootCount=0,groupCount=0,rootStage=0,mainControls=0;
    bool GatherRoot(CollisionObject* object);
    static unsigned FloatControls();
    CollisionDetector mainDetector,workerDetector;
    LightEvent wake,finished;
    Thread thread=nullptr;
    unsigned next=0,count=0,state=0,quit=0,workerAreas=0,mainAreas=0,workerCore=~0u;
    float dt=0;
    bool attempted=false,verificationFailed=false;
    ParallelCollisionState();
    ~ParallelCollisionState();
    bool Initialize();
    void Run(CollisionDetector& detector,bool worker);
    void Dispatch();
    bool CaptureVolume(CollisionVolume* volume);
    bool CheckVolumes() const;
    static void Worker(void* argument);
};
}
#endif
