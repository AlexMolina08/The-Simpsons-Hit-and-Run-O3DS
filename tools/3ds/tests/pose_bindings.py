#!/usr/bin/env python3
"""Check bounded blend bindings against uncached lookups and asset replacement."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <p3d/anim/posebindings.hpp>
struct Group { uint64_t uid; unsigned identity; };
struct Animation {
    std::vector<Group> groups;
    mutable unsigned calls = 0;
    const Group* GetGroupByUID(uint64_t uid) const {
        ++calls;
        for(const auto& group:groups) if(group.uid == uid) return &group;
        return nullptr;
    }
};
static uint32_t randomState = 0x8331983;
static unsigned Next() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    return randomState ^= randomState << 5;
}
int main() {
    static_assert(sizeof(Old3dsPoseBindings<Animation,Group>) <= 2080,
                  "Scratch holds 256 pointers and 32 bytes of validity bits");
    uint64_t comparisons=0, referenceCalls=0, cachedCalls=0;
    Animation animations[8];
    for(unsigned test=0;test<4000;++test) {
        unsigned count=1+Next()%8, joints=1+Next()%160;
        std::vector<uint64_t> uids(joints);
        std::vector<bool> preserve(joints);
        for(unsigned joint=0;joint<joints;++joint) {
            uids[joint]=(uint64_t(Next()) << 32) | Next();
            preserve[joint]=(Next()%4==0);
        }
        for(auto& animation:animations) {
            // Reuse the same animation addresses with new contents each call.
            animation.groups.clear(); animation.calls=0;
            for(unsigned joint=0;joint<joints;++joint) if(Next()%3) {
                animation.groups.push_back({uids[joint],Next()});
                if(Next()%8==0) animation.groups.push_back({uids[joint],Next()});
            }
        }
        Old3dsPoseBindings<Animation,Group> bindings;
        for(unsigned pass=0;pass<2;++pass) {
            for(unsigned joint=0;joint<joints;++joint) {
                if(pass==0 && preserve[joint]) continue;
                for(unsigned slot=0;slot<count;++slot) {
                    auto& animation=animations[slot];
                    const Group* expected=animation.GetGroupByUID(uids[joint]);
                    ++referenceCalls;
                    const unsigned before=animation.calls;
                    const Group* result=bindings.Get(&animation,slot,joint,uids[joint]);
                    cachedCalls+=animation.calls-before;
                    assert(result==expected); ++comparisons;
                }
            }
        }
    }
    assert(cachedCalls < referenceCalls);
    printf("PASS: %llu binding comparisons; lookups %llu reference / %llu reused; "
           "missing and duplicate UIDs, preserved lengths, capacity fallback and asset replacement\n",
           (unsigned long long)comparisons,(unsigned long long)referenceCalls,
           (unsigned long long)cachedCalls);
}
'''
with tempfile.TemporaryDirectory(prefix="srr-pose-bindings-") as temporary:
    folder = Path(temporary)
    (folder / "test.cpp").write_text(source)
    subprocess.run([
        "clang++", "-std=c++14", "-O2", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-I", str(root / "libs/pure3d"),
        str(folder / "test.cpp"), "-o", str(folder / "test"),
    ], check=True)
    subprocess.run([str(folder / "test")], check=True)
