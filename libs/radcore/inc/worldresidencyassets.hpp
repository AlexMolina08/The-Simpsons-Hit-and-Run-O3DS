#ifndef SRR_WORLD_RESIDENCY_ASSETS_HPP
#define SRR_WORLD_RESIDENCY_ASSETS_HPP
#include <cstdint>
#include <cstring>
#ifdef SRR2_OLD3DS_WORLD_RESIDENCY
extern "C" {
extern const char* g_old3dsResidencyRoot;
bool old3dsWorldResidencyAdmit(unsigned level);
// Zero requeues a pending open; positive admits the complete level; negative rejects it.
int old3dsWorldResidencyAdmitStep(unsigned level);
}
#endif
// Canonical retail exterior files. R4 splits into A/B in levels 1 and 4.
// Keep admission and read-only asset resolution on the same allowlist.
namespace WorldResidencyAssets {
static const char* const Files[]={"L1_TERRA.p3d","L4_TERRA.p3d","L7_TERRA.p3d","l1r1.p3d","l1r2.p3d","l1r3.p3d","l1r4a.p3d","l1r4b.p3d","l1r6.p3d","l1r7.p3d","l1z1.p3d","l1z2.p3d","l1z3.p3d","l1z4.p3d","l1z6.p3d","l1z7.p3d","l2_TERRA.p3d","l2r1.p3d","l2r2.p3d","l2r3.p3d","l2r4.p3d","l2z1.p3d","l2z2.p3d","l2z3.p3d","l2z4.p3d","l3_TERRA.p3d","l3r1.p3d","l3r2.p3d","l3r3.p3d","l3r4.p3d","l3r5.p3d","l3z1.p3d","l3z2.p3d","l3z3.p3d","l3z4.p3d","l3z5.p3d","l4r1.p3d","l4r2.p3d","l4r3.p3d","l4r4a.p3d","l4r4b.p3d","l4r6.p3d","l4r7.p3d","l4z1.p3d","l4z2.p3d","l4z3.p3d","l4z4.p3d","l4z6.p3d","l4z7.p3d","l5_TERRA.p3d","l5r1.p3d","l5r2.p3d","l5r3.p3d","l5r4.p3d","l5z1.p3d","l5z2.p3d","l5z3.p3d","l5z4.p3d","l6_TERRA.p3d","l6r1.p3d","l6r2.p3d","l6r3.p3d","l6r4.p3d","l6r5.p3d","l6z1.p3d","l6z2.p3d","l6z3.p3d","l6z4.p3d","l6z5.p3d","l7r1.p3d","l7r2.p3d","l7r3.p3d","l7r6.p3d","l7r7.p3d","l7z1.p3d","l7z2.p3d","l7z3.p3d","l7z6.p3d","l7z7.p3d"};
inline unsigned Level(const char* name){return (name[0]=='l' || name[0]=='L') && name[1]>='1' && name[1]<='7' ? unsigned(name[1]-'0') : 0;}
inline bool Terrain(const char* name){return std::strstr(name,"_TERRA")!=nullptr;}
inline std::uint64_t LowerUID(const char* text){
    std::uint64_t value=0;for(;*text;++text){unsigned char c=*text;if(c>='A' && c<='Z')c+='a'-'A';value=(value*65599)^c;}return value;
}
inline bool Sector(unsigned level,std::uint64_t uid){
    for(const char* name:Files)if(Level(name)==level && !Terrain(name) && LowerUID(name)==uid)return true;
    return false;
}
}
#endif
