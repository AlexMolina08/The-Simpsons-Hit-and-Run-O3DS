#ifdef SRR2_OLD3DS_PARALLEL_COLLISION
#include "simcollision/parallelcollision.hpp"
#include "simcollision/collisionmanager.hpp"
#include "simcollision/collisionvolume.hpp"
#include <cstring>
#include <typeinfo>
#include <simcommon/simstatearticulated.hpp>

unsigned g_old3dsParallelCollisionMode=0;
namespace {Old3dsParallelCollisionStats statistics={};unsigned ownerRejects=0,typeRejects=0,stateRejects=0;const char* rejectedType="none";}
extern "C" void old3dsParallelCollisionGetStats(Old3dsParallelCollisionStats* output){*output=statistics;}
extern "C" void old3dsParallelCollisionDump(FILE* f)
{
    const auto& s=statistics;
    fprintf(f,"NATIVE_PARALLEL_GUARD owner_rejects=%u type_rejects=%u state_rejects=%u last_type=%s\n",ownerRejects,typeRejects,stateRejects,rejectedType);
    fprintf(f,"NATIVE_PARALLEL_ROOTS roots=%u worker_groups=%u main_groups=%u audit_ms=%.6f root_ms=%.6f\n",s.roots,s.workerGroups,s.mainGroups,s.auditTicks*1000.0/SYSCLOCK_ARM11,s.rootTicks*1000.0/SYSCLOCK_ARM11);
    fprintf(f,"NATIVE_PARALLEL_COLLISION calls=%u areas=%u pairs=%u worker_areas=%u main_areas=%u cancelled=%u overflows=%u fallbacks=%u verified_areas=%u verified_pairs=%u verified_contacts=%u mismatches=%u volume_mismatches=%u initial_allowance=%u actual_allowance=%u worker_core=%u init_result=%08lx thread_failed=%u prepare_ms=%.6f execute_ms=%.6f join_ms=%.6f total_ms=%.6f\n",s.calls,s.areas,s.pairs,s.workerAreas,s.mainAreas,s.cancelled,s.overflows,s.fallbacks,s.verifiedAreas,s.verifiedPairs,s.verifiedContacts,s.mismatches,s.volumeMismatches,s.initialAllowance,s.actualAllowance,s.workerCore,(unsigned long)s.initResult,s.threadFailed,s.prepareTicks*1000.0/SYSCLOCK_ARM11,s.executeTicks*1000.0/SYSCLOCK_ARM11,s.joinTicks*1000.0/SYSCLOCK_ARM11,s.totalTicks*1000.0/SYSCLOCK_ARM11);
}
namespace sim {
ParallelCollisionState::ParallelCollisionState()
{
    LightEvent_Init(&wake,RESET_ONESHOT);LightEvent_Init(&finished,RESET_ONESHOT);
}
ParallelCollisionState::~ParallelCollisionState()
{
    if(thread) {
        __atomic_store_n(&quit,1u,__ATOMIC_RELEASE);LightEvent_Signal(&wake);
        threadJoin(thread,U64_MAX);threadFree(thread);
        APT_SetAppCpuTimeLimit(statistics.initialAllowance?statistics.initialAllowance:30);
    }
}
bool ParallelCollisionState::Initialize()
{
    if(attempted)return thread && !verificationFailed;
    attempted=true;u32 initial=0;APT_GetAppCpuTimeLimit(&initial);statistics.initialAllowance=initial;
    statistics.initResult=APT_SetAppCpuTimeLimit(80);
    if(R_FAILED(static_cast<Result>(statistics.initResult)))return false;
    u32 actual=0;APT_GetAppCpuTimeLimit(&actual);statistics.actualAllowance=actual;
    s32 priority=0x30;svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);
    if(priority<0x3f)++priority;
    thread=threadCreate(Worker,this,32768,priority,1,false);
    if(!thread){statistics.threadFailed=1;APT_SetAppCpuTimeLimit(initial?initial:30);}
    return thread!=nullptr;
}
void ParallelCollisionState::Worker(void* argument)
{
    auto& s=*static_cast<ParallelCollisionState*>(argument);
    for(;;) {
        LightEvent_Wait(&s.wake);
        if(__atomic_load_n(&s.quit,__ATOMIC_ACQUIRE))return;
        unsigned queued=1;
        if(!__atomic_compare_exchange_n(&s.state,&queued,2u,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))continue;
        s.workerCore=svcGetProcessorID();
        s.Run(s.workerDetector,true);
        __atomic_store_n(&s.state,3u,__ATOMIC_RELEASE);LightEvent_Signal(&s.finished);
    }
}
unsigned ParallelCollisionState::FloatControls()
{
    unsigned value;__asm__ volatile("vmrs %0, fpscr":"=r"(value));
    return value&0x07f79f00u;
}
void ParallelCollisionState::Run(CollisionDetector& detector,bool worker)
{
    // Never change floating-point controls. Incompatible workers leave every
    // job for main, preserving the original thread's arithmetic environment.
    if(rootStage && worker && FloatControls()!=mainControls)return;
    for(;;) {
        const unsigned index=__atomic_fetch_add(&next,1u,__ATOMIC_RELAXED);
        if(index>=count)break;
        if(rootStage) {
            for(unsigned r=groups[index].first;r!=MaxVolumes;r=roots[r].next)
                roots[r].volume->UpdateAll();
        } else {
            Job& j=jobs[index];
            j.overflow=!detector.DetectPreparedArea(j.pairs,j.contacts,dt);
            j.tail=detector.PreparedTailCount();
        }
        if(worker)++workerAreas;else ++mainAreas;
    }
}
void ParallelCollisionState::Dispatch()
{
    next=0;workerAreas=mainAreas=0;
    __atomic_store_n(&state,1u,__ATOMIC_RELEASE);LightEvent_Signal(&wake);
    Run(mainDetector,false);
    unsigned queued=1;
    if(__atomic_compare_exchange_n(&state,&queued,0u,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)) {
        // Main finished before the worker acquired the batch. Cancel without
        // waiting for a system-core scheduling slice. A late wake sees no job.
        ++statistics.cancelled;
    } else {
        const u64 start=svcGetSystemTick();LightEvent_Wait(&finished);
        statistics.joinTicks+=svcGetSystemTick()-start;
        __atomic_store_n(&state,0u,__ATOMIC_RELEASE);
    }
    if(rootStage){statistics.mainGroups+=mainAreas;statistics.workerGroups+=workerAreas;}
    else {statistics.mainAreas+=mainAreas;statistics.workerAreas+=workerAreas;}
}
namespace {
unsigned VolumeValues(CollisionVolume* v,float* out)
{
    unsigned n=0;
    const auto vector=[&](const rmt::Vector& p){out[n++]=p.x;out[n++]=p.y;out[n++]=p.z;};
    vector(v->mPosition);vector(v->mBoxSize);out[n++]=v->mSphereRadius;out[n++]=v->IsVisible()?1.f:0.f;out[n++]=float(v->Type());
    switch(v->Type()) {
    case CylinderVolumeType:{const auto* c=static_cast<CylinderVolume*>(v);vector(c->mAxis);out[n++]=c->mLength;out[n++]=c->mCylinderRadius;out[n++]=c->mFlatEnd?1.f:0.f;out[n++]=float(c->mAxisOrientation);break;}
    case OBBoxVolumeType:{const auto* b=static_cast<OBBoxVolume*>(v);for(unsigned i=0;i<3;++i)vector(b->mAxis[i]);for(unsigned i=0;i<3;++i)out[n++]=b->mLength[i];out[n++]=float(b->mAxisOrientation);break;}
    case WallVolumeType:{const auto* w=static_cast<WallVolume*>(v);vector(w->mNormal);out[n++]=float(w->mAxisOrientation);break;}
    default:break;
    }
    return n;
}
bool SameVector(const rmt::Vector& a,const rmt::Vector& b)
{
    return !std::memcmp(&a.x,&b.x,sizeof(float)) && !std::memcmp(&a.y,&b.y,sizeof(float)) && !std::memcmp(&a.z,&b.z,sizeof(float));
}
bool SameContact(const Collision& a,const Collision& b)
{
    return a.mCollisionVolumeA==b.mCollisionVolumeA && a.mCollisionVolumeB==b.mCollisionVolumeB &&
        a.mCollisionObjectA==b.mCollisionObjectA && a.mCollisionObjectB==b.mCollisionObjectB &&
        a.mIndexA==b.mIndexA && a.mIndexB==b.mIndexB && a.mStatus==b.mStatus &&
        !std::memcmp(&a.mDistance,&b.mDistance,sizeof(float)) && SameVector(a.mNormal,b.mNormal) &&
        SameVector(a.GetPositionA(),b.GetPositionA()) && SameVector(a.GetPositionB(),b.GetPositionB());
}
}
bool ParallelCollisionState::SupportedVolume(CollisionVolume* v,CollisionObject* owner,unsigned depth)
{
    if(!v || depth>64 || v->GetCollisionObject()!=owner){++ownerRejects;return false;}
    unsigned slot=(reinterpret_cast<uintptr_t>(v)>>4)*2654435761u;
    for(unsigned probe=0;probe<16;++probe) {
        const unsigned index=(slot+probe)&4095;
        if(safeStamps[index]==safeGeneration) {if(safeVolumes[index]==v)return true;continue;}
        const auto& type=typeid(*v);
        if(type!=typeid(CollisionVolume) && type!=typeid(SphereVolume) && type!=typeid(CylinderVolume) &&
           type!=typeid(OBBoxVolume) && type!=typeid(WallVolume) && type!=typeid(BBoxVolume)){++typeRejects;rejectedType=type.name();return false;}
        safeStamps[index]=safeGeneration;safeVolumes[index]=v;
        if(auto* children=v->SubVolumeList())for(int i=0;i<children->GetSize();++i)
            if(!SupportedVolume((*children)[i],owner,depth+1))return false;
        return true;
    }
    return false;
}
bool ParallelCollisionState::GatherRoot(CollisionObject* object)
{
    if(object->IsStatic())return true;
    CollisionVolume* volume=object->GetCollisionVolume();
    for(unsigned i=0;i<rootCount;++i)if(roots[i].volume==volume)return true;
    if(rootCount==MaxVolumes || !SupportedVolume(volume,object))return false;
    SimState* state=object->GetSimState();
    if(!state)return false;
    void* key=state;
    if(typeid(*state)==typeid(SimStateArticulated)) {
        // Joint world transforms resolve lazily and mutate parent/child flags.
        // All roots sharing a pose must stay in one sequential group.
        key=static_cast<SimStateArticulated*>(state)->GetPose();
        if(!key)return false;
    } else if(typeid(*state)!=typeid(SimState) && typeid(*state)!=typeid(ManualSimState)){++stateRejects;rejectedType=typeid(*state).name();return false;}
    // ManualSimState returns the immutable identity matrix. Manual volume
    // rotation/bounds writes still belong exclusively to this root's group.
    unsigned group=0;while(group<groupCount && groups[group].key!=key)++group;
    roots[rootCount]={volume,MaxVolumes};
    if(group==groupCount) {groups[group]={key,rootCount,rootCount};++groupCount;}
    else {roots[groups[group].last].next=rootCount;groups[group].last=rootCount;}
    ++rootCount;return true;
}
bool ParallelCollisionState::CaptureVolume(CollisionVolume* v)
{
    for(unsigned i=0;i<volumeCount;++i)if(volumes[i].volume==v)return true;
    if(volumeCount==MaxVolumes)return false;
    auto& snapshot=volumes[volumeCount++];snapshot.volume=v;snapshot.updated=v->Updated();snapshot.count=VolumeValues(v,snapshot.values);
    if(auto* children=v->SubVolumeList())for(int i=0;i<children->GetSize();++i)if(!CaptureVolume((*children)[i]))return false;
    return true;
}
bool ParallelCollisionState::CheckVolumes() const
{
    for(unsigned i=0;i<volumeCount;++i) {
        const auto& s=volumes[i];float values[24];const unsigned count=VolumeValues(s.volume,values);
        if(count!=s.count || s.updated!=s.volume->Updated() || std::memcmp(values,s.values,count*sizeof(float)))return false;
    }
    return true;
}
bool CollisionManager::DetectParallel(float dt,float gameTime,bool printOut)
{
    if(!mParallelCollision){++statistics.fallbacks;return false;}
    auto& s=*mParallelCollision;
    if(printOut || s.verificationFailed || mCacheActivatedArea.GetSize()>int(s.MaxAreas) || !s.Initialize()) {++statistics.fallbacks;return false;}
    const u64 start=svcGetSystemTick();
    if(!++s.safeGeneration){std::memset(s.safeStamps,0,sizeof(s.safeStamps));++s.safeGeneration;}
    // Only dynamic roots are written. Static trees remain read-only and need
    // no recursive ownership audit. Reject unknown writers before mutation.
    s.rootCount=s.groupCount=0;
    for(int i=0;i<mCacheActivatedArea.GetSize();++i) {
        const Area& area=mAreaArray[mCacheActivatedArea[i]];if(!area.mActivated)continue;
        for(int p=0;p<area.mPairList.GetSize();++p) {
            const auto& pair=area.mPairList.GetAt(p);
            if(!s.GatherRoot(pair.mObjectA) || !s.GatherRoot(pair.mObjectB)) {++statistics.fallbacks;return false;}
        }
    }
    statistics.auditTicks+=svcGetSystemTick()-start;
    ++statistics.calls;
    const unsigned previousBatch=mCollisionDetector.BeginPreparationBatch();
    s.count=0;s.dt=dt;
    // No solver, gameplay callback or pose writer runs between this preparation
    // and the join. The kernels below read geometry and append to private areas.
    for(int i=0;i<mCacheActivatedArea.GetSize();++i) {
        Area& a=mAreaArray[mCacheActivatedArea[i]];if(!a.mActivated)continue;
        a.PrepareForCollisionDetection(mCMAttributes);
        auto& job=s.jobs[s.count++];job.pairs=&a.mPairList;job.contacts=&a.mCollisionList;job.updatedPair=false;
        for(int p=0;p<a.mPairList.GetSize();++p)job.updatedPair|=a.mPairList[p].mNeedsUpdate;

        statistics.pairs+=a.mPairList.GetSize();
    }
    statistics.areas+=s.count;statistics.prepareTicks+=svcGetSystemTick()-start;
    const unsigned areaCount=s.count;
    // Gather only roots required by the newly prepared flags. The earlier
    // conservative audit guarantees this pass cannot fail after mutation.
    s.rootCount=s.groupCount=0;
    for(unsigned a=0;a<areaCount;++a) {
        auto& pairs=*s.jobs[a].pairs;
        for(int p=0;p<pairs.GetSize();++p)if(pairs[p].mNeedsUpdate) {
            s.GatherRoot(pairs[p].mObjectA);s.GatherRoot(pairs[p].mObjectB);
        }
    }
    s.mainControls=s.FloatControls();s.rootStage=1;s.count=s.groupCount;
    const u64 rootStart=svcGetSystemTick();s.Dispatch();
    statistics.rootTicks+=svcGetSystemTick()-rootStart;statistics.roots+=s.rootCount;
    s.rootStage=0;s.count=areaCount;
    s.mainDetector.CopyPreparedConfiguration(mCollisionDetector);s.workerDetector.CopyPreparedConfiguration(mCollisionDetector);
    const u64 execute=svcGetSystemTick();s.Dispatch();statistics.executeTicks+=svcGetSystemTick()-execute;
    if(s.workerAreas)statistics.workerCore=s.workerCore;
    unsigned jobIndex=0;
    // Only after all readers join may fallback allocation or self-collision
    // update volumes. Preserve each area's contact list and its pair order.
    for(int i=0;i<mCacheActivatedArea.GetSize();++i) {
        Area& a=mAreaArray[mCacheActivatedArea[i]];if(!a.mActivated)continue;
        auto& job=s.jobs[jobIndex++];
        if(job.overflow) {
            ++statistics.overflows;a.mCollisionList.Clear();
            mCollisionDetector.DetectCollision(&a.mPairList,&a.mCollisionList,dt,false);
        } else mCollisionDetector.RestorePreparedTail(dt,a.mPairList.GetSize()!=0,job.tail,job.updatedPair);
        // Direct self-collision calls inherit the last pair's filtering tail.
        // Set the original detector's destination even for empty pair lists.
        mCollisionDetector.SetPreparedDestination(&a.mCollisionList);
        for(int o=0;o<a.mObjectList.GetSize();++o) {
            CollisionObject* object=a.mObjectList[o];if(!object->GetSelfCollisionEnabled())continue;
            for(int j=0;j<object->GetSelfCollisionList().GetSize();++j) {
                auto& sc=object->GetSelfCollisionList()[j];sc.mCollisionVolume1->UpdateAll();sc.mCollisionVolume2->UpdateAll();
                mCollisionDetector.DetectCollision(sc.mCollisionVolume1,sc.mCollisionVolume2,&a.mCollisionList);
            }
        }
    }
    mCollisionDetector.EndPreparationBatch(previousBatch);
    statistics.totalTicks+=svcGetSystemTick()-start;
    if(g_old3dsParallelCollisionMode==2) {
        // Keep candidate data owned by the manager; original detection then
        // replaces the live lists before any solver consumes them.
        bool captured=true;s.volumeCount=0;
        for(unsigned i=0;i<s.count;++i) {
            auto& job=s.jobs[i];job.candidate.Clear();job.candidateFlags.Clear();
            const int n=job.contacts->GetSize(),pairs=job.pairs->GetSize();
            if(n>4096 || pairs>4096){captured=false;break;}
            if(job.candidate.ArraySize()<n)job.candidate.ResizeArray(n);
            if(job.candidateFlags.ArraySize()<pairs)job.candidateFlags.ResizeArray(pairs);
            for(int c=0;c<n;++c)job.candidate.Add((*job.contacts)[c]);
            for(int p=0;p<pairs;++p) {auto& pair=(*job.pairs)[p];job.candidateFlags.Add({pair.mCollide,pair.mNeedsUpdate,pair.mSafeTimeBeforeCollision});captured&=s.CaptureVolume(pair.VolumeA());captured&=s.CaptureVolume(pair.VolumeB());}
        }
        for(int i=0;i<mCacheActivatedArea.GetSize();++i) {
            Area& area=mAreaArray[mCacheActivatedArea[i]];if(!area.mActivated)continue;
            for(int o=0;o<area.mObjectList.GetSize();++o)captured&=s.CaptureVolume(area.mObjectList[o]->GetCollisionVolume());
        }
        const unsigned mode=g_old3dsParallelCollisionMode;g_old3dsParallelCollisionMode=0;
        DetectCollision(dt,gameTime,false);g_old3dsParallelCollisionMode=mode;
        if(captured) {
            for(unsigned i=0;i<s.count;++i) {
                auto& job=s.jobs[i];++statistics.verifiedAreas;
                if(job.candidate.GetSize()!=job.contacts->GetSize())++statistics.mismatches;
                else for(int c=0;c<job.candidate.GetSize();++c) {++statistics.verifiedContacts;if(!SameContact(job.candidate[c],(*job.contacts)[c]))++statistics.mismatches;}
                for(int p=0;p<job.pairs->GetSize();++p) {++statistics.verifiedPairs;const auto& saved=job.candidateFlags[p];const auto& current=(*job.pairs)[p];if(saved.contacts!=current.mCollide || saved.needsUpdate!=current.mNeedsUpdate || std::memcmp(&saved.safeTime,&current.mSafeTimeBeforeCollision,sizeof(float)))++statistics.mismatches;}
            }
            if(!s.CheckVolumes())++statistics.volumeMismatches;
        } else {++statistics.fallbacks;++statistics.mismatches;}
        if(statistics.mismatches || statistics.volumeMismatches)s.verificationFailed=true;
        return true;
    }
    EndDetectCollision();return true;
}
}
#endif
