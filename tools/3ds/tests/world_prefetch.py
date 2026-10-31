#!/usr/bin/env python3
"""Exercise production speculative-load ownership with serialized loader mocks."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'code/render/RenderManager/RenderManager.cpp').read_text()
start = source.index('bool RenderManager::RetainWorldZone')
end = source.index('\n#endif\nvoid RenderManager::DoPostLevelLoad', start)
methods = source[start:end]
start = source.index('   if(pUserData==&s_prefetchCallback)')
end = source.index('\n#endif', start)
callback = source[start:end]
start = source.index('             if(s_prefetchZone[0])')
end = source.index('\n             for(int n=0;n<pZEL->GetNumLoadZones();++n)\n                 WorldStreaming::Event("demand"', start)
consume = source[start:end]
prelude = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <set>
#include <functional>
#include "code/main/streamingpolicy.h"
#include "code/loading/worldpriority.h"
using tUID=unsigned long long;
struct tName {
 std::string name; explicit tName(const char* p):name(p){}
 static tUID MakeUID(const char* p){return std::hash<std::string>{}(p);}
 tUID GetUID()const{return MakeUID(name.c_str());}
};
template<class T>struct Array {
 std::vector<T> data;int mUseSize=0;
 T operator[](unsigned i){return data.at(i);}
 void RemoveKeepOrder(unsigned i){data.erase(data.begin()+i);--mUseSize;}
 void ClearUse(){data.clear();mUseSize=0;}
 void add(T p){data.push_back(p);++mUseSize;}
};
struct ZoneEventLocator {
 bool interior=false,interiorDump=false;std::vector<std::string> load,dump;
 bool IsInteriorLoad(){return interior;}bool IsInteriorDump(){return interiorDump;}
 int GetNumLoadZones(){return load.size();}int GetNumDumpZones(){return dump.size();}
 const char* GetLoadZone(int i){return load.at(i).c_str();}const char* GetDumpZone(int i){return dump.at(i).c_str();}
};
static unsigned normalUsed=0,linearFree=8u<<20,now=0;
struct Mall {unsigned uordblks;};static Mall mallinfo(){return {normalUsed};}
static unsigned linearSpaceFree(){return linearFree;}
static unsigned radTimeGetMilliseconds(){return now;}
static unsigned __ctru_heap_size=30u<<20;
static char s_prefetchZone[96]={};static bool s_prefetchLoading=false;
static int s_prefetchCallback=0;static unsigned s_prefetchReadyAt=0;static bool s_prefetchRetained=false;
namespace WorldStreaming {
 bool predictive=true,worldRetention=false;static std::vector<std::string> events;
 bool Admit(unsigned n,unsigned c,unsigned a,unsigned b){return StreamingPolicy::Admit(n,c,a,b);}
 void Event(const char* s,const char*){events.push_back(s);}
}
namespace RenderEnums {enum {LevelSlot=1,IgnoreGuts=2};}
enum {GMA_LEVEL_ZONE=1,FILEHANDLER_PURE3D=1,EVENT_DYNAMIC_ZONE_LOAD_ENDED=2,EVENT_NAMED_DYNAMIC_ZONE_LOAD_ENDED=3,EVENT_LOCATOR=100};
namespace LocatorEvent {enum {DYNAMIC_ZONE=1};}using EventEnum=int;
struct Wrapper {
 enum {msBillboard=0};static Wrapper* GetInstance(){static Wrapper w;return &w;}
 Wrapper& mLoader(int){return *this;}void ModRegdListener(void*,int){}
};using AllWrappers=Wrapper;
struct Events {std::vector<int> sent;void TriggerEvent(int id,void* =nullptr){sent.push_back(id);}};
static Events events;static Events* GetEventManager(){return &events;}
struct WorldRenderLayer {
 std::set<tUID> loaded;unsigned posts=0,starts=0,drops=0,extra=0;
 bool HasDynaLoad(tUID id){return loaded.count(id);}
 unsigned SectionCount(){return loaded.size()+extra;}unsigned SectionCapacity(){return 7;}
 bool DoPreDynaLoad(tName& n){++starts;return loaded.insert(n.GetUID()).second;}
 void DoPostDynaLoad(){++posts;}
 void DumpDynaLoad(tName& n,Array<void*>&){++drops;loaded.erase(n.GetUID());}
};
struct Loader {
 bool busy=false;unsigned requests=0;std::string file;void* callback=nullptr;
 bool IsLoading(){return busy;}
 void AddRequest(int,const char* f,int,const char*,void*,void*,void* cb){busy=true;++requests;file=f;callback=cb;}
};static Loader loader;static Loader* GetLoadingManager(){return &loader;}
struct RenderManager {
 bool mbDynaLoading=false,mDoneInitialLoad=true,mbLoadZonesDumped=false,mbDrivingTooFastLoad=false;
 Array<ZoneEventLocator*> mZELs;Array<void*> mEntityDeletionList;WorldRenderLayer world;
 ZoneEventLocator* dispatched=nullptr;
 WorldRenderLayer* pWorldRenderLayer(){return &world;}
 void RedirectChunks(int){}
 void HandleEvent(int,ZoneEventLocator* p){assert(!mbDynaLoading);dispatched=p;}
 bool CanPrefetchZone(ZoneEventLocator*);
 bool RetainWorldZone(const char*,ZoneEventLocator*);
 void UpdateWorldPrefetch(ZoneEventLocator*,unsigned);
 void Complete(void* pUserData);
 void Consume(ZoneEventLocator* pZEL);
};
'''
test = r'''
static void reset(){loader=Loader{};normalUsed=0;linearFree=8u<<20;s_prefetchZone[0]=0;s_prefetchLoading=false;s_prefetchRetained=false;events.sent.clear();now=0;WorldStreaming::worldRetention=false;WorldStreaming::predictive=true;}
int main(){
 ZoneEventLocator zone;zone.load={"l2r1.p3d"};
 reset();RenderManager m;
 m.UpdateWorldPrefetch(nullptr,0);assert(!loader.requests);
 zone.interior=true;assert(!m.CanPrefetchZone(&zone));zone.interior=false;
 // An exterior gate can also request an interior dump at actual entry. Prediction
 // loads only its exterior file and must not deliver that dump event early.
 zone.interiorDump=true;assert(m.CanPrefetchZone(&zone));zone.interiorDump=false;
 m.world.extra=6;m.UpdateWorldPrefetch(&zone,0);assert(!loader.requests);m.world.extra=0;
 normalUsed=25u<<20;m.UpdateWorldPrefetch(&zone,0);assert(!loader.requests);normalUsed=0;
 m.UpdateWorldPrefetch(&zone,0);assert(loader.requests==1&&s_prefetchLoading&&m.mbDynaLoading);
 assert(loader.file=="art\\l2r1.p3d"&&loader.callback==&s_prefetchCallback);
 m.UpdateWorldPrefetch(&zone,10);assert(loader.requests==1);
 loader.busy=false;m.Complete(loader.callback);assert(!s_prefetchLoading&&!m.mbDynaLoading&&m.world.posts==1);
 assert(events.sent.empty()); // Prediction must not deliver demand callbacks.
 m.Consume(&zone);assert(!s_prefetchZone[0]&&m.world.loaded.size()==1);
 assert(events.sent==std::vector<int>({EVENT_DYNAMIC_ZONE_LOAD_ENDED,EVENT_NAMED_DYNAMIC_ZONE_LOAD_ENDED}));
 m.UpdateWorldPrefetch(&zone,100);assert(loader.requests==1); // Already resident.
 reset();RenderManager expiry;expiry.UpdateWorldPrefetch(&zone,0);loader.busy=false;expiry.Complete(loader.callback);
 expiry.UpdateWorldPrefetch(nullptr,3999);assert(!expiry.world.drops);
 expiry.UpdateWorldPrefetch(nullptr,4000);assert(expiry.world.drops==1&&!s_prefetchZone[0]);
 reset();RenderManager unrelated;unrelated.UpdateWorldPrefetch(&zone,0);loader.busy=false;unrelated.Complete(loader.callback);
 ZoneEventLocator other;other.load={"l2z1.p3d"};unrelated.Consume(&other);
 assert(unrelated.world.drops==1&&unrelated.world.loaded.empty()&&events.sent.empty());
 reset();RenderManager queued;queued.UpdateWorldPrefetch(&zone,0);queued.mZELs.add(&other);
 loader.busy=false;queued.Complete(loader.callback);assert(queued.dispatched==&other&&queued.mZELs.mUseSize==0&&queued.mbDrivingTooFastLoad);
 reset();RenderManager cancelled;cancelled.UpdateWorldPrefetch(&zone,0);cancelled.mZELs.add(&other);cancelled.mbLoadZonesDumped=true;
 loader.busy=false;cancelled.Complete(loader.callback);assert(!cancelled.dispatched&&!s_prefetchZone[0]&&!cancelled.mZELs.mUseSize);
 reset();RenderManager warm;warm.world.loaded={tName::MakeUID("l2z2.p3d"),tName::MakeUID("l2r1.p3d")};
 assert(!warm.RetainWorldZone("l2z2.p3d",&zone));
 WorldStreaming::worldRetention=true;WorldStreaming::predictive=false;
 assert(warm.RetainWorldZone("l2z2.p3d",&zone)&&s_prefetchRetained);
 assert(!warm.RetainWorldZone("l2r1.p3d",&zone)); // One extra section across both policies.
 warm.UpdateWorldPrefetch(nullptr,19999);assert(!warm.world.drops);
 ZoneEventLocator revisit;revisit.load={"l2z2.p3d"};warm.Consume(&revisit);
 assert(!s_prefetchRetained&&!s_prefetchZone[0]&&events.sent.empty()); // No duplicate load event for an already loaded file.
 assert(warm.world.loaded.size()==2&&!loader.requests);
 assert(warm.RetainWorldZone("l2z2.p3d",&zone));warm.UpdateWorldPrefetch(nullptr,20000);
 assert(warm.world.drops==1&&!s_prefetchZone[0]);
 reset();WorldStreaming::worldRetention=true;RenderManager pressure;
 pressure.world.loaded={tName::MakeUID("l2z2.p3d"),tName::MakeUID("l2r1.p3d")};
 assert(!pressure.RetainWorldZone("l2z2.p3d",&other)); // Incoming demand must already fit without a new load.
 assert(pressure.RetainWorldZone("l2z2.p3d",&zone));linearFree=(2u<<20)-1;
 pressure.UpdateWorldPrefetch(nullptr,1);assert(pressure.world.drops==1&&!s_prefetchRetained);
 reset();WorldStreaming::worldRetention=true;RenderManager competition;
 competition.world.loaded={tName::MakeUID("l2z2.p3d"),tName::MakeUID("l2r1.p3d")};
 assert(competition.RetainWorldZone("l2z2.p3d",&zone));competition.Consume(&other);
 assert(competition.world.drops==1&&!s_prefetchRetained&&events.sent.empty());
}
'''
code = prelude + methods + '\nvoid RenderManager::Complete(void* pUserData){\n' + callback + '\n}\n'
code += 'void RenderManager::Consume(ZoneEventLocator* pZEL){\n' + consume + '\n}\n' + test
with tempfile.TemporaryDirectory(prefix='world-prefetch-') as td:
    p = Path(td)
    (p / 'test.cpp').write_text(code)
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-fsanitize=address,undefined',
                    '-I', str(root), str(p / 'test.cpp'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('PASS: production prefetch admission, single-flight callback, deferred demand events, expiry, unrelated-zone retirement, queued demand and cancellation')
