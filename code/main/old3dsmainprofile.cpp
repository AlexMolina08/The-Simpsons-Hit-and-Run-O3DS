#include <main/old3dsmainprofile.h>
#ifdef SRR2_OLD3DS_MAIN_PROFILE
#include <cstring>
#include <cstdint>
namespace Old3dsMainProfile {
void* mainTls=nullptr;bool enabled=false;
namespace {
struct Node {u64 inclusive=0,exclusive=0;unsigned calls=0,directChildren=0,descendants=0;};
struct Record {unsigned frame=0,routeMs=0,scopes=0,errors=0;u64 work=0,roots=0;Node nodes[Count];};
constexpr unsigned Capacity=16,Stride=64;
struct Edge {u64 ticks=0;unsigned calls=0;};
Edge edges[Count+1][Count];
struct Site {const void* caller=nullptr;unsigned stage=0,calls=0;u64 ticks=0;};
Site sites[512];unsigned siteOverflow=0;
Record records[Capacity],current;
unsigned count=0,overwritten=0;u64 emptyScopeTicks=0;
bool requested=false,focusDrop=false;Scope* active=nullptr;
const char* names[]={"game_update","world_layer","world_scene","opaque","translucent","geometry","composite","poly_skin",
    "p3d_pose_evaluate","p3d_pose_joint","matrix_multiply","draw_prim","apply_material","material_protocol","material_pass",
    "material_capture","material_prerender","uniform_capture","light_rebuild","prim_display","geometry_capture",
    "begin_prepared","flush_prepared","frame_begin","viewport","vehicle_update","store_joints","physics_update",
    "physics_substep","characters_update","poser_assign","poser_fill",
    "hud_interface","hud_radar","hud_sprite","hud_font","hud_wrap","hud_art"};
static_assert(sizeof(names)/sizeof(names[0])==Count,"Every profiler stage needs one label");
}
void Configure(bool value)
{
    requested=value;__atomic_store_n(&mainTls,getThreadLocalStorage(),__ATOMIC_RELAXED);
    if(value){
        current={};active=nullptr;__atomic_store_n(&enabled,true,__ATOMIC_RELAXED);
        const u64 begin=svcGetSystemTick();
        for(unsigned i=0;i<1024;++i){Scope calibration(MatrixMultiply);}
        emptyScopeTicks=svcGetSystemTick()-begin;
        __atomic_store_n(&enabled,false,__ATOMIC_RELAXED);current={};std::memset(edges,0,sizeof(edges));std::memset(sites,0,sizeof(sites));
    }
}
void FocusDropWindow(bool value){focusDrop=value;}
void BeginFrame(unsigned frame,bool collecting,unsigned routeMs)
{
    const bool selected=requested && collecting && (focusDrop
        ? routeMs>=15000 && routeMs<18000 && frame%4==1 : frame%Stride==17);
    __atomic_store_n(&enabled,selected,__ATOMIC_RELAXED);
    if(selected){current={};current.frame=frame;current.routeMs=routeMs;active=nullptr;}
}
void Scope::Enter()
{
    parent=active;active=this;start=svcGetSystemTick();
}
void Scope::Leave()
{
    const u64 span=svcGetSystemTick()-start;
    if(active!=this || stage>=Count){++current.errors;active=parent;return;}
    auto& node=current.nodes[stage];node.inclusive+=span;node.exclusive+=span>=children?span-children:0;
    ++node.calls;++current.scopes;node.directChildren+=directChildren;node.descendants+=descendants;
    auto& edge=edges[parent?parent->stage:Count][stage];edge.ticks+=span;++edge.calls;
    unsigned slot=((reinterpret_cast<std::uintptr_t>(caller)>>2)^(stage*0x9e3779b9u))&511u;
    unsigned probe=0;for(;probe<512;++probe){auto& site=sites[slot];if(!site.calls || (site.caller==caller && site.stage==stage)){site.caller=caller;site.stage=stage;site.ticks+=span;++site.calls;break;}slot=(slot+1)&511u;}
    if(probe==512)++siteOverflow;
    if(parent){parent->children+=span;++parent->directChildren;parent->descendants+=1+descendants;}else current.roots+=span;
    active=parent;
}
void EndFrame(u64 work)
{
    if(!__atomic_load_n(&enabled,__ATOMIC_RELAXED))return;
    __atomic_store_n(&enabled,false,__ATOMIC_RELAXED);current.work=work;if(active)++current.errors;
    records[count%Capacity]=current;if(++count>Capacity)++overwritten;active=nullptr;
}
void Dump(FILE* file,double tickPairNs)
{
    fprintf(file,"NATIVE_MAIN_PROFILE_ANCHOR address=%p site_overflow=%u\n",reinterpret_cast<void*>(&Configure),siteOverflow);
    const double emptyNs=emptyScopeTicks*1e9/SYSCLOCK_ARM11/1024;
    fprintf(file,"NATIVE_MAIN_PROFILE requested=%u stride=%u records=%u overwritten=%u focused_drop=%u window_start_ms=15000 window_end_ms=18000 window_stride=4 main_tls_only=1 tick_pair_ns=%.3f empty_scope_ns=%.3f diagnostic_only=1 nested_times_not_additive=1 calibrated_body_is_estimate=1\n",requested,Stride,count<Capacity?count:Capacity,overwritten,focusDrop,tickPairNs,emptyNs);
    for(unsigned i=count>Capacity?count-Capacity:0;i<count;++i){
        const auto& record=records[i%Capacity];
        fprintf(file,"NATIVE_MAIN_PROFILE_FRAME frame=%u route_ms=%u work_ms=%.6f roots_ms=%.6f outside_roots_ms=%.6f scopes=%u errors=%u\n",record.frame,record.routeMs,record.work*1000.0/SYSCLOCK_ARM11,record.roots*1000.0/SYSCLOCK_ARM11,(record.work>record.roots?record.work-record.roots:0)*1000.0/SYSCLOCK_ARM11,record.scopes,record.errors);
        for(unsigned stage=0;stage<Count;++stage){const auto& node=record.nodes[stage];if(!node.calls)continue;
            const double inclusiveMs=node.inclusive*1000.0/SYSCLOCK_ARM11,exclusiveMs=node.exclusive*1000.0/SYSCLOCK_ARM11;
            const double estimatedBody=inclusiveMs-(node.descendants*emptyNs+node.calls*tickPairNs/2)/1e6;
            const double estimatedExclusive=exclusiveMs-(node.directChildren*(emptyNs-tickPairNs/2)+node.calls*tickPairNs/2)/1e6;
            fprintf(file,"NATIVE_MAIN_PROFILE_NODE frame=%u name=%s calls=%u inclusive_ms=%.6f exclusive_ms=%.6f child_scopes=%u descendant_scopes=%u estimated_body_ms=%.6f estimated_exclusive_ms=%.6f\n",record.frame,names[stage],node.calls,inclusiveMs,exclusiveMs,node.directChildren,node.descendants,estimatedBody>0?estimatedBody:0,estimatedExclusive>0?estimatedExclusive:0);
        }
    }
    for(unsigned parent=0;parent<=Count;++parent)for(unsigned child=0;child<Count;++child){const auto& edge=edges[parent][child];if(!edge.calls)continue;
        fprintf(file,"NATIVE_MAIN_PROFILE_EDGE parent=%s child=%s calls=%u inclusive_ms=%.6f\n",parent==Count?"root":names[parent],names[child],edge.calls,edge.ticks*1000.0/SYSCLOCK_ARM11);
    }

    for(const auto& site:sites)if(site.calls)
        fprintf(file,"NATIVE_MAIN_PROFILE_SITE stage=%s return_pc=%p calls=%u inclusive_ms=%.6f\n",names[site.stage],site.caller,site.calls,site.ticks*1000.0/SYSCLOCK_ARM11);

}
}
#endif
