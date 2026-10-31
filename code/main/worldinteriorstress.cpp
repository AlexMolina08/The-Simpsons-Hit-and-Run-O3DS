#include <main/worldinteriorstress.h>
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <render/Loaders/AllWrappers.h>
#include <main/old3dsmemory.h>
#endif
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <contexts/context.h>
#include <events/eventmanager.h>
#include <events/eventlistener.h>
#include <events/eventdata.h>
#include <interiors/interiormanager.h>
#include <loading/loadingmanager.h>
#include <meta/interiorentrancelocator.h>
#include <meta/zoneeventlocator.h>
#include <meta/triggervolumetracker.h>
#include <meta/triggervolume.h>
#include <mission/gameplaymanager.h>
#include <render/IntersectManager/IntersectManager.h>
#include <render/RenderManager/RenderManager.h>
#include <render/RenderManager/WorldRenderLayer.h>
#include <worldsim/avatarmanager.h>
#include <worldsim/character/character.h>
#include <ai/sequencer/actioncontroller.h>
#include <worldsim/avatar.h>
#include <worldresidencyassets.hpp>
#include <3ds.h>
#include <cstdio>
#include <cmath>

namespace WorldInteriorStress {
namespace {
struct Observer:EventListener {
    tUID loaded=0;
    void HandleEvent(EventEnum id,void* data) override {
        if(id==EVENT_INTERIOR_DUMPED)loaded=0;
        if(id==EVENT_INTERIOR_LOADED && data)loaded=static_cast<InteriorLoadedEventData*>(data)->interiorName.GetUID();
    }
};
static Observer* observer=nullptr;
static InteriorEntranceLocator* entrance=nullptr;
static ZoneEventLocator* preload=nullptr;
static ZoneEventLocator* exterior=nullptr;
static unsigned activePhase=0,stage=0,dwell=0;
static u64 started=0;
static bool retained=false;
static void Release() {
    if(retained){entrance->Release();preload->Release();exterior->Release();}
    entrance=nullptr;preload=nullptr;exterior=nullptr;retained=false;
}
static bool Ground(const char* where) {
    auto* avatar=GetAvatarManager()->GetAvatarForPlayer(0);if(!avatar || !avatar->GetCharacter())return false;
    rmt::Vector position;avatar->GetPosition(position);rmt::Vector probe=position,normal,ground;probe.y+=2.f;bool hit=false;
    GetIntersectManager()->FindIntersection(probe,hit,normal,ground);
    const bool valid=hit && std::isfinite(ground.y) && std::fabs(ground.y-position.y)<4.f;
    std::printf("NATIVE_INTERIOR_GROUND phase=%u location=%s hit=%u valid=%u x=%.5f y=%.5f z=%.5f ground_y=%.5f\n",activePhase,where,hit,valid,position.x,position.y,position.z,hit?ground.y:0.f);
    return valid;
}
static bool Start() {
    if(!observer){observer=new(GMA_PERSISTENT) Observer();GetEventManager()->AddListener(observer,EVENT_INTERIOR_LOADED);GetEventManager()->AddListener(observer,EVENT_INTERIOR_DUMPED);}
    auto* avatar=GetAvatarManager()->GetAvatarForPlayer(0);if(!avatar || !avatar->GetCharacter() || GetInteriorManager()->IsInside())return false;
    auto* tracker=GetTriggerVolumeTracker();rmt::Vector from;avatar->GetPosition(from);float nearest=1.e30f;
    for(unsigned i=0;i<tracker->GetNumTriggers();++i){
        auto* locator=tracker->GetTrigger(i)->GetLocator();if(!locator || locator->GetDataType()!=LocatorType::INTERIOR_ENTRANCE)continue;
        auto* candidate=static_cast<InteriorEntranceLocator*>(locator);rmt::Vector position;candidate->GetPosition(&position);
        for(unsigned j=0;j<tracker->GetNumTriggers();++j){
            auto* other=tracker->GetTrigger(j)->GetLocator();if(!other || other->GetDataType()!=LocatorType::DYNAMIC_ZONE)continue;
            auto* zone=static_cast<ZoneEventLocator*>(other);
            if(!zone->IsInteriorLoad())continue;
            // Entrance labels and loaded interior identities can differ (the
            // bar is authored as moe/moe1). Pair the adjacent authored locators.
            rmt::Vector zonePosition;zone->GetPosition(&zonePosition);
            if((zonePosition-position).MagnitudeSqr()>64.f)continue;
            const float distance=(position-from).MagnitudeSqr();if(distance<nearest){nearest=distance;entrance=candidate;preload=zone;}
        }
    }
    if(!entrance || !preload)return false;
    rmt::Vector position;entrance->GetPosition(&position);nearest=1.e30f;const unsigned map=unsigned(GetGameplayManager()->GetCurrentLevelIndex())+1;
    for(unsigned i=0;i<tracker->GetNumTriggers();++i){
        auto* volume=tracker->GetTrigger(i);auto* locator=volume->GetLocator();if(!locator || locator->GetDataType()!=LocatorType::DYNAMIC_ZONE)continue;
        auto* zone=static_cast<ZoneEventLocator*>(locator);if(zone->IsInteriorLoad() || zone->IsInteriorDump() || !zone->GetNumLoadZones())continue;
        bool valid=true;for(int j=0;j<zone->GetNumLoadZones();++j)valid=valid && WorldResidencyAssets::Sector(map,WorldResidencyAssets::LowerUID(zone->GetLoadZone(j)));
        const float distance=(volume->GetPosition()-position).MagnitudeSqr();if(valid && distance<nearest){nearest=distance;exterior=zone;}
    }
    if(!exterior)return false;
    entrance->AddRef();preload->AddRef();exterior->AddRef();retained=true;
    auto* character=avatar->GetCharacter();if(character->IsInCar())GetAvatarManager()->PutCharacterOnGround(character,character->GetTargetVehicle());
    GetRenderManager()->mpLayer(RenderEnums::LevelSlot)->DumpAllDynaLoads(1,GetRenderManager()->mEntityDeletionList);
    GetAllWrappers()->DumpIntegrity(stdout);Old3dsMemoryDump(stdout);
    exterior->Trigger(0,true);preload->Trigger(0,true);
    character->RelocateAndReset(position,0.f);
    std::printf("NATIVE_INTERIOR_ACTION phase=%u action=preload name=%s exterior_distance=%.3f\n",activePhase,entrance->GetInteriorFileName(),std::sqrt(nearest));
    return true;
}
}
int Step(unsigned phase,unsigned elapsed,int context) {
    if(activePhase!=phase){Release();activePhase=phase;stage=dwell=0;started=svcGetSystemTick();}
    if(svcGetSystemTick()-started>120ULL*SYSCLOCK_ARM11){std::printf("NATIVE_INTERIOR_ACTION phase=%u action=timeout stage=%u\n",phase,stage);return -1;}
    auto* interior=GetInteriorManager();
    if(stage==0){
        if(context!=CONTEXT_GAMEPLAY || GetLoadingManager()->IsLoading() || interior->IsEntering() || interior->IsExiting())return 0;
        // Some authored starts, including Level 4, place the player inside.
        // Leave through the normal exit flow before testing an exterior entry.
        if(interior->IsInside()){GetEventManager()->TriggerEvent(EVENT_EXIT_INTERIOR_START);stage=6;std::printf("NATIVE_INTERIOR_ACTION phase=%u action=initial_exit\n",phase);return 0;}
        if(!Start()){std::printf("NATIVE_INTERIOR_ACTION phase=%u action=start_failed\n",phase);return -1;}
        stage=1;return 0;
    }
    if(GetLoadingManager()->IsLoading())return 0;
    if(stage==6){if(!interior->IsInside() && !interior->IsExiting())stage=0;return 0;}
    if(stage==1 && observer->loaded==preload->GetUID()){
        auto* character=GetAvatarManager()->GetAvatarForPlayer(0)->GetCharacter();character->GetActionController()->Clear();
        interior->Enter(entrance,character,character->GetActionController()->GetNextSequencer());stage=2;
        std::printf("NATIVE_INTERIOR_ACTION phase=%u action=enter\n",phase);
    }else if(stage==2 && interior->IsInside() && !interior->IsEntering()){
        if(!Ground("inside"))return -1;GetAllWrappers()->DumpIntegrity(stdout);Old3dsMemoryDump(stdout);stage=3;dwell=0;
    }else if(stage==3){
        dwell+=elapsed;if(dwell>=2000){GetEventManager()->TriggerEvent(EVENT_EXIT_INTERIOR_START);stage=4;std::printf("NATIVE_INTERIOR_ACTION phase=%u action=exit\n",phase);}
    }else if(stage==4 && !interior->IsInside() && !interior->IsExiting()){
        if(!Ground("outside"))return -1;stage=5;dwell=0;
    }else if(stage==5){dwell+=elapsed;if(dwell>=2000){std::printf("NATIVE_INTERIOR_ACTION phase=%u action=complete\n",phase);Release();return 1;}}
    return 0;
}
}
#endif
