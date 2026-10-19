#ifndef PICA_SKINSELECTION_HPP
#define PICA_SKINSELECTION_HPP
#include <cstring>
namespace picaSkin
{
inline bool SelectPedestrian(const char* mesh, const char* skeleton)
{
    // Audited generic pedestrian meshes: all groups fit 24 bones and each full
    // skin uses less than 40 KiB. Named mission characters keep their CPU path.
    static const char* const names[] = {
        "boy1", "boy2", "boy3", "boy4", "girl1", "girl2", "girl3", "girl4",
        "fem1", "fem2", "fem3", "fem4", "male3", "male4", "busm1", "busm2",
        "busw1", "const1", "const2", "joger1", "joger2", "olady1", "olady2",
        "olady3", "rednk1", "rednk2", "farmr1"
    };
    if(!mesh || !skeleton) return false;
    for(unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        if(std::strcmp(skeleton, names[i]) != 0) continue;
        const unsigned length = std::strlen(names[i]);
        return std::strncmp(mesh, names[i], length) == 0 &&
               std::strcmp(mesh + length, "_h_merged") == 0;
    }
    return false;
}

inline bool ExactCastPair(const char* mesh, const char* skeleton, const char* const* names, unsigned count)
{
    if(!mesh || !skeleton) return false;
    for(unsigned i = 0; i < count; ++i)
    {
        if(std::strcmp(skeleton, names[i]) != 0) continue;
        const unsigned length = std::strlen(names[i]);
        return std::strncmp(mesh, names[i], length) == 0 &&
               std::strcmp(mesh + length, "_h_merged") == 0;
    }
    return false;
}
inline bool SelectAdditionalPlayer(const char* mesh, const char* skeleton)
{
    static const char* const names[] = { "lisa", "marge", "apu" };
    return ExactCastPair(mesh, skeleton, names, sizeof(names) / sizeof(names[0]));
}
inline bool SelectAdditionalPedestrian(const char* mesh, const char* skeleton)
{
    static const char* const names[] = {
        "male1", "male2", "male5", "male6", "bum", "hooker", "mobstr", "nuclear",
        "sail1", "sail2", "sail3", "sail4", "zfem1", "zfem5", "zmale1", "zmale3"
    };
    return ExactCastPair(mesh, skeleton, names, sizeof(names) / sizeof(names[0]));
}
inline bool SelectNamedCharacter(const char* mesh, const char* skeleton)
{
    // Audited full skins fit 25 bones per group and the separate 64 KiB NPC pool.
    static const char* const names[] = {
        "milhouse", "ned", "nelson", "moe", "barney", "burns", "lenny", "carl",
        "skinner", "willie", "cletus", "frink", "patty", "selma", "lou", "hibbert",
        "otto", "jimbo", "kearney", "dolph", "jasper", "moleman", "smithers",
        "krusty", "nriviera", "louie", "teen", "snake", "gil"
    };
    return ExactCastPair(mesh, skeleton, names, sizeof(names) / sizeof(names[0]));
}
inline bool SelectCostumePlayer(const char* mesh, const char* skeleton)
{
    // Exact audited costume pairs; each complete skin fits the existing player
    // pool, including allocation headers and rigid/weighted vertex layouts.
    static const char* const names[] = {
        "h_stcrobe", "h_undrwr", "h_scuzzy", "h_evil", "b_ninja", "b_man",
        "b_football", "b_hugo", "b_military", "a_american", "a_army", "m_police",
        "l_jersey", "brn_unf"
    };
    return ExactCastPair(mesh, skeleton, names, sizeof(names) / sizeof(names[0]));
}
inline bool SelectCostumePedestrian(const char* mesh, const char* skeleton)
{
    static const char* const names[] = { "zmale4" };
    return ExactCastPair(mesh, skeleton, names, sizeof(names) / sizeof(names[0]));
}
inline bool SelectCostumeNamed(const char* mesh, const char* skeleton)
{
    static const char* const names[] = { "witch" };
    return ExactCastPair(mesh, skeleton, names, sizeof(names) / sizeof(names[0]));
}

inline bool SelectPackedPlayer(const char* mesh, const char* skeleton)
{
    static const char* const names[] = {
        "m_prison", "m_pink", "h_fat", "l_florida", "a_besharp", "l_cool", "h_donut", "b_tall"
    };
    return ExactCastPair(mesh, skeleton, names, sizeof(names) / sizeof(names[0]));
}
inline bool SelectPackedNamed(const char* mesh, const char* skeleton)
{
    static const char* const names[] = {
        "cbg", "ralph", "wiggum", "beeman", "eddie", "frankenstein", "captain", "grandpa", "askinner"
    };
    return ExactCastPair(mesh, skeleton, names, sizeof(names) / sizeof(names[0]));
}

}
#endif
