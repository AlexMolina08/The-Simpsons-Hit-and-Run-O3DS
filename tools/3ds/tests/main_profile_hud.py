#!/usr/bin/env python3
"""Exercise production HUD call-tree nesting, labels and disabled observers."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
code = r'''
#include <cassert>
#include <cstdio>
#include <main/old3dsmainprofile.h>
static unsigned ticks=10, reads=0;
static void* tls=(void*)1;
u64 svcGetSystemTick(){++reads;return ticks+=10;}
void* getThreadLocalStorage(){return tls;}
#include "SOURCE"
int main(){
 using namespace Old3dsMainProfile;
 Configure(true);BeginFrame(17,true);
 {Scope ui(HudInterface);
  {Scope radar(HudRadar);Scope sprite(HudSprite);Scope primitive(PrimDisplay);}
  {Scope layout(HudWrap);Scope font(HudFont);}
  {Scope art(HudArt);}
  tls=(void*)2;{Scope otherThread(HudSprite);assert(otherThread.start==0);}tls=(void*)1;
 }
 EndFrame(10000);Dump(stdout,10);
 Configure(false);BeginFrame(81,true);unsigned before=reads;
 {Scope disabled(HudInterface);assert(disabled.start==0);}
 EndFrame(10000);assert(reads==before);
 Configure(true);FocusDropWindow(true);
 BeginFrame(17,true,14999);assert(!enabled);
 BeginFrame(21,true,15000);assert(enabled);{Scope focused(HudInterface);}EndFrame(10000);
 BeginFrame(22,true,16000);assert(!enabled);
 BeginFrame(25,true,17999);assert(enabled);{Scope focused(HudInterface);}EndFrame(10000);
 BeginFrame(29,true,18000);assert(!enabled);
 FocusDropWindow(false);BeginFrame(81,true,20000);assert(enabled);EndFrame(10000);
 Configure(false);
}
'''.replace("SOURCE", str(root / "code/main/old3dsmainprofile.cpp"))
with tempfile.TemporaryDirectory(prefix="srr-hud-profile-") as directory:
    p = Path(directory)
    (p / "3ds.h").write_text(
        "#pragma once\n#include <cstdint>\nusing u64=uint64_t;\n"
        "constexpr unsigned SYSCLOCK_ARM11=1000000;\n"
        "u64 svcGetSystemTick();void* getThreadLocalStorage();\n")
    (p / "test.cpp").write_text(code)
    subprocess.run(["clang++", "-std=c++14", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-DSRR2_OLD3DS_MAIN_PROFILE",
                    "-I" + str(p), "-I" + str(root / "code"), str(p / "test.cpp"),
                    "-o", str(p / "test")], check=True)
    output = subprocess.check_output([str(p / "test")], text=True)
    assert "errors=0" in output
    for name in ("hud_interface", "hud_radar", "hud_sprite", "hud_font", "hud_wrap", "hud_art"):
        assert f"name={name} calls=1 " in output, name
    assert "parent=hud_radar child=hud_sprite calls=1 " in output
    assert "parent=hud_sprite child=prim_display calls=1 " in output
print("PASS: production HUD labels and nesting, zero accounting errors, wrong-thread isolation and zero clock reads when disabled.")
