#!/usr/bin/env python3
"""Execute production pacing: preserve Old sleep policy and refresh-sync New 60 Hz."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "code/main/game.cpp").read_text()
start = source.index("        if(Graphics3DS::IsNew() && Graphics3DS::TargetFPS() == 60)")
body = source[start:source.index("\n#endif", start)]
program = r'''
#include <main/graphics3ds.h>
#include <cassert>
using u64=unsigned long long;using s64=long long;
const u64 SYSCLOCK_ARM11=268123480;
unsigned vblanks=0,sleeps=0;s64 lastSleep=0;
void gspWaitForVBlank(){++vblanks;}
void svcSleepThread(s64 ns){++sleeps;lastSleep=ns;}
void pace(u64 frameTicks){
 const u64 frameBudgetTicks=SYSCLOCK_ARM11/Graphics3DS::TargetFPS();
''' + body + r'''
}
int main(){
 using namespace Graphics3DS;
 Initialize(Old3DS);Complete(true);pace(0);
 assert(!vblanks&&sleeps==1&&lastSleep>33333000&&lastSleep<33334000);
 pace(SYSCLOCK_ARM11);assert(!vblanks&&sleeps==1);
 RequestQuality(true);Complete(true);pace(0);assert(!vblanks&&sleeps==2);
 Initialize(Old2DS);Complete(true);pace(0);assert(!vblanks&&sleeps==3);
 Initialize(New3DS);Complete(true);pace(0);assert(vblanks==1&&sleeps==3);
 RequestQuality(false);Complete(true);pace(0);assert(vblanks==2&&sleeps==3);
 pace(SYSCLOCK_ARM11);assert(vblanks==3&&sleeps==3);
 Initialize(New2DSXL);Complete(true);RequestQuality(false);Complete(true);pace(0);
 assert(vblanks==4&&sleeps==3);
 Initialize(255);Complete(true);pace(0);assert(vblanks==4&&sleeps==4);
}
'''
with tempfile.TemporaryDirectory(prefix="srr-pacing-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(program)
    binary = path / "test"
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-I", str(root / "code"), str(path / "test.cpp"),
        str(root / "code/main/graphics3ds.cpp"), str(root / "code/cheats/portsettings.cpp"),
        "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], cwd=path, check=True)
print("PASS: production Old/Quality sleep, over-budget handling, New display-refresh pacing and unknown-model fallback.")
