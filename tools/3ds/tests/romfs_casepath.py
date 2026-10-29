#!/usr/bin/env python3
"""Exercise the production RomFS case resolver against a case-sensitive fixture."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
header = root / 'libs/radcore/src/radfile/sdl/casepath.hpp'
fixture = r'''
#include <dirent.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
static std::map<std::string, std::vector<std::string>> tree = {
 {"romfs:/", {"scripts", "Art"}},
 {"romfs:/scripts/", {"cars"}},
 {"romfs:/scripts/cars/", {"pursuit", "PursuiT"}},
 {"romfs:/scripts/cars/pursuit/", {"L1cop.con"}},
 {"romfs:/scripts/cars/PursuiT/", {"Other.con"}},
 {"romfs:/Art/", {"Title.P3D"}},
};
static std::vector<std::string>* current;
static size_t entryIndex;
static dirent result;
static DIR* fakeOpen(const char* name) {
 auto it=tree.find(name);
 if(it==tree.end()) return nullptr;
 current=&it->second; entryIndex=0; return reinterpret_cast<DIR*>(1);
}
static dirent* fakeRead(DIR*) {
 if(entryIndex==current->size()) return nullptr;
 std::strcpy(result.d_name, (*current)[entryIndex++].c_str()); return &result;
}
static int fakeClose(DIR*) {return 0;}
#define opendir fakeOpen
#define readdir fakeRead
#define closedir fakeClose
#include HEADER
int main() {
 char out[257];
 assert(srr2LowercaseDirectories("romfs:/scripts/cars/Pursuit/L1cop.con", out, sizeof out));
 assert(!std::strcmp(out, "romfs:/scripts/cars/pursuit/L1cop.con"));
 assert(!srr2LowercaseDirectories("romfs:/scripts/cars/pursuit/L1cop.con", out, sizeof out));
 assert(!srr2LowercaseDirectories("romfs:/scripts/cars/Pursuit/L1cop.con", out, 10));
 assert(srr2ResolveCasePath("romfs:/", "scripts/cars/pursuit/L1cop.con", out, sizeof out));
 assert(!std::strcmp(out, "romfs:/scripts/cars/pursuit/L1cop.con"));
 assert(!srr2ResolveCasePath("romfs:/", "scripts/cars/Pursuit/L1cop.con", out, sizeof out));
 tree["romfs:/scripts/cars/"].pop_back();
 assert(srr2ResolveCasePath("romfs:/", "SCRIPTS/cars/Pursuit/l1COP.CON", out, sizeof out));
 assert(!std::strcmp(out, "romfs:/scripts/cars/pursuit/L1cop.con"));
 assert(srr2ResolveCasePath("romfs:/", "art/title.p3d", out, sizeof out));
 assert(!std::strcmp(out, "romfs:/Art/Title.P3D"));
 assert(!srr2ResolveCasePath("romfs:/", "art/missing.p3d", out, sizeof out));
 assert(!srr2ResolveCasePath("romfs:/", "../art/title.p3d", out, sizeof out));
 assert(!srr2ResolveCasePath("romfs:/", "art//title.p3d", out, sizeof out));
 assert(!srr2ResolveCasePath("romfs:/", "art/title.p3d", out, 12));
 assert(!srr2ResolveCasePath("romfs:/", "art/", out, sizeof out));
 puts("PASS: RomFS exact, case mismatch, ambiguous names, missing files and path bounds");
}
'''
with tempfile.TemporaryDirectory(prefix='romfs-case-') as directory:
    source = Path(directory) / 'test.cpp'
    source.write_text(fixture.replace('HEADER', '"' + str(header) + '"'))
    binary = Path(directory) / 'test'
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
