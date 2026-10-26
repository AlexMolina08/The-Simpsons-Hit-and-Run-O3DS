#include <main/old3dsrecurring.h>
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#ifdef SRR2_OLD3DS_RECURRING_PROFILE
#include <main/old3dsdiag.h>
#include <render/DSG/IEntityDSG.h>
#include <camera/supercammanager.h>
#include <camera/supercamcentral.h>
#include <p3d/pointcamera.hpp>
#include <cstring>
namespace Old3dsRecurring {
bool active=false,requested=false,focusDropWindow=false;unsigned stationaryMs=0;
unsigned windowStartMs=15000,windowEndMs=18000;
namespace {
// Keep the focused trace inside the shared handheld startup budget. Asset
// aggregates retain every selected sample even if the frame ring wraps.
constexpr unsigned Capacity=16,Assets=256;
struct Node {u64 inclusive=0,exclusive=0;unsigned calls=0;};
struct Record {unsigned frame=0,ms=0,scopes=0,errors=0;Node nodes[Count];};
struct Asset {unsigned long long uid=0;unsigned category=0,pass=0,calls=0,draws=0,vertices=0;char name[40]={};u64 ticks=0,backend=0;float pixelSum=0;};
Record records[Capacity],current;Asset assets[Assets+2*O3_CATEGORY_COUNT],families[2][O3_CATEGORY_COUNT];
unsigned count=0,overflow=0,backendDepth=0;Scope* top=nullptr;u64 backendTicks=0,calibration=0;
const char* names[]={"update","render","gameplay","avatar","footprint","garbage","effects","physics","pedestrian","trigger","traffic","actor","interior","skidmark","coin","hitrun","presentation","playing","world","visibility","list","opaque","translucent","shadow","backend","material","submit","uniforms","physics_areas","physics_ground","physics_pre_sim","physics_vehicle_prep","physics_character_prep","physics_actions","physics_detect","physics_solve","physics_vehicles","physics_characters","physics_camera","physics_dynamics","vehicle_pre_step","vehicle_controller","character_pre_sim","static_query","dynamic_query","animated_query","joint_transforms","joint_inertia","detect_prepare","detect_pairs","detect_volumes","detect_wall","detect_box","detect_cylinder","detect_sphere","character_controller","character_actions","character_puppet","particle_update","particle_sprites","billboard_display"};
static_assert(sizeof(names)/sizeof(names[0])==Count,"Every recurring profiler stage needs one label");
}
static bool BackendStage(unsigned stage){return stage==Backend || stage==Submit || stage==Material || stage==Uniforms;}
void Scope::Enter(){parent=top;top=this;start=svcGetSystemTick();if(BackendStage(stage))++backendDepth;}
void Scope::Leave(){const u64 span=svcGetSystemTick()-start;auto& n=current.nodes[stage];n.inclusive+=span;n.exclusive+=span>=children?span-children:0;++n.calls;++current.scopes;if(top!=this)++current.errors;if(parent)parent->children+=span;if(BackendStage(stage) && --backendDepth==0)backendTicks+=span;top=parent;}
void Begin(unsigned frame,unsigned ms,bool collecting){
    active=requested && collecting && (focusDropWindow
        ? ms>=windowStartMs && ms<windowEndMs && frame%4==1 : frame%32==17);
    if(!active)return;
    if(!calibration){const u64 begin=svcGetSystemTick();for(unsigned i=0;i<1024;++i){Scope s(Update);}calibration=svcGetSystemTick()-begin;}
    current={};current.frame=frame;current.ms=ms;top=nullptr;backendTicks=0;backendDepth=0;
}
void End(){if(!active)return;if(top)++current.errors;records[count++%Capacity]=current;active=false;top=nullptr;}
static unsigned FindAssetSlot(unsigned long long uid,unsigned category,unsigned pass){
    // Static scenery can fill the table before the translucent animation pass.
    // Reserve half for animated scenery in focused captures, without adding RAM.
    const unsigned size=focusDropWindow?Assets/2:Assets;
    const unsigned base=focusDropWindow && (category==O3_ANIM || category==O3_PARTICLE)?Assets/2:0;
    const unsigned limit=focusDropWindow?size:8;
    unsigned i=unsigned(uid^(uid>>32)^pass)&(size-1);
    for(unsigned probe=0;probe<limit;++probe){
        const unsigned slot=base+i;const auto& a=assets[slot];
        if(!a.calls || (a.uid==uid && a.pass==pass && a.category==category))return slot;
        i=(i+1)&(size-1);
    }
    ++overflow;return Assets+pass*O3_CATEGORY_COUNT+category;
}
Entity::Entity(IEntityDSG* entity,unsigned pass){
    if(!active)return;
    const unsigned long long uid=entity->GetUID();const unsigned category=entity->Old3dsCategory();
    const unsigned i=FindAssetSlot(uid,category,pass);
    auto& a=assets[i];if(!a.calls){a.uid=uid;a.pass=pass;a.category=category;snprintf(a.name,sizeof(a.name),"%s",entity->GetName()?entity->GetName():"unnamed");}++a.calls;slot=i;
    rmt::Sphere sphere;entity->GetBoundingSphere(&sphere);
    rmt::Vector camera;GetSuperCamManager()->GetSCC(0)->GetCamera()->GetPosition(&camera);
    const float distance=(sphere.centre-camera).Magnitude();
    if(distance>0)a.pixelSum+=240.f*sphere.radius/distance;
    draws=g_old3dsDrawCalls;vertices=g_old3dsDrawVerts;backend=backendTicks;start=svcGetSystemTick();
}
Entity::~Entity(){if(!start)return;auto& a=assets[slot];a.ticks+=svcGetSystemTick()-start;a.backend+=backendTicks-backend;a.draws+=g_old3dsDrawCalls-draws;a.vertices+=g_old3dsDrawVerts-vertices;auto& family=families[a.pass][a.category];++family.calls;family.ticks+=svcGetSystemTick()-start;family.backend+=backendTicks-backend;family.draws+=g_old3dsDrawCalls-draws;family.vertices+=g_old3dsDrawVerts-vertices;}
void Dump(FILE* f){
    fprintf(f,"NATIVE_RECURRING enabled=%u stride=32 focused_drop=%u window_start_ms=%u window_end_ms=%u window_stride=4 records=%u overwritten=%u empty_scope_ns=%.3f asset_overflow=%u animated_asset_reserve=%u stationary_ms=%u diagnostic_only=1 no_frame_io=1 projected_radius_approx=240*r/d material_groups_are_submitted_draws=1\n",requested,focusDropWindow,windowStartMs,windowEndMs,count<Capacity?count:Capacity,count>Capacity?count-Capacity:0,calibration*1e9/SYSCLOCK_ARM11/1024,overflow,focusDropWindow?Assets/2:0,stationaryMs);
    for(unsigned i=count>Capacity?count-Capacity:0;i<count;++i){const auto& r=records[i%Capacity];fprintf(f,"NATIVE_RECURRING_FRAME frame=%u route_ms=%u scopes=%u errors=%u\n",r.frame,r.ms,r.scopes,r.errors);for(unsigned s=0;s<Count;++s){const auto& n=r.nodes[s];if(n.calls)fprintf(f,"NATIVE_RECURRING_NODE frame=%u name=%s calls=%u inclusive_ms=%.6f exclusive_ms=%.6f\n",r.frame,names[s],n.calls,n.inclusive*1000.0/SYSCLOCK_ARM11,n.exclusive*1000.0/SYSCLOCK_ARM11);}}
    for(unsigned pass=0;pass<2;++pass)for(unsigned category=0;category<O3_CATEGORY_COUNT;++category){const auto& a=families[pass][category];if(a.calls)fprintf(f,"NATIVE_RECURRING_FAMILY category=%u pass=%u calls=%u draws=%u vertices=%u inclusive_ms=%.6f backend_ms=%.6f\n",category,pass,a.calls,a.draws,a.vertices,a.ticks*1000.0/SYSCLOCK_ARM11,a.backend*1000.0/SYSCLOCK_ARM11);}
    for(unsigned slot=0;slot<Assets+2*O3_CATEGORY_COUNT;++slot){const auto& a=assets[slot];if(a.calls)fprintf(f,"NATIVE_RECURRING_ASSET slot=%u family_overflow=%u uid=%016llx name=%s category=%u pass=%u calls=%u draws=%u vertices=%u inclusive_ms=%.6f backend_ms=%.6f projected_radius_mean=%.3f\n",slot,slot>=Assets,slot>=Assets?0:a.uid,slot>=Assets?"UNATTRIBUTED_FAMILY":a.name,a.category,a.pass,a.calls,a.draws,a.vertices,a.ticks*1000.0/SYSCLOCK_ARM11,a.backend*1000.0/SYSCLOCK_ARM11,a.pixelSum/a.calls); }
}
}
#else
namespace Old3dsRecurring {
bool active=false,requested=false,focusDropWindow=false;unsigned stationaryMs=0;
unsigned windowStartMs=15000,windowEndMs=18000;
void Begin(unsigned,unsigned,bool){}void End(){}
void Dump(FILE* f){fprintf(f,"NATIVE_RECURRING enabled=0 compiled=0\n");}
}
#endif
#endif
