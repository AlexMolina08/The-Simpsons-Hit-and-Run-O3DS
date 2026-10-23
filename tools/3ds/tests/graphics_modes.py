#!/usr/bin/env python3
"""Exercise production model policy, preference isolation and failed activation."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = r'''
#include <main/graphics3ds.h>
#include <cheats/portsettings.h>
#include <cassert>
#include <cstring>
using namespace Graphics3DS;
int main(int argc, char** argv) {
    if(argc > 1 && !strcmp(argv[1], "read")) {
        Initialize(Old3DS); assert(!RequestedQuality());
        Initialize(Old2DS); assert(RequestedQuality());
        Initialize(New3DS); assert(!RequestedQuality());
        assert(PortSettings::NoTimers()); return 0;
    }
    for(int m = 0; m <= 6; ++m) {
        Initialize(m); const bool newer = m == 2 || m == 4 || m == 5;
        assert(IsNew() == newer);
        assert(SupportsWide() == (m != 3 && m != 6));
        assert(SupportsStereo() == (m != 3 && m != 5 && m != 6));
        assert(RequestedQuality() == (m != 6)); Complete(true);
        assert(RenderWidth() == (SupportsWide() ? 800u : 400u));
        assert(TargetFPS() == (newer ? 60u : 30u));
        if(SupportsWide()) assert(!strcmp(Status(), newer ? "800x240 / 60 FPS TARGET" : "800x240 / 30 FPS TARGET"));
    }
    for(int m = 0; m <= 6; ++m) {
        Initialize(m); const bool newer = m == 2 || m == 4 || m == 5;
        RequestQuality(false); Complete(true);
        assert(TargetFPS() == (newer ? 60u : 30u));
        RequestQuality(true); Complete(true);
        assert(TargetFPS() == (newer ? 60u : 30u));
        assert(RenderWidth() == (SupportsWide() ? 800u : 400u));
        assert(ImprovedLighting() == (m == 3));
        if(SupportsStereo()) {
            UpdateSlider(1); Complete(true);
            assert(Stereo() && RenderWidth() == 400 && TargetFPS() == (newer ? 60u : 30u));
            UpdateSlider(0); Complete(true);
            assert(Quality() && RenderWidth() == 800 && TargetFPS() == (newer ? 60u : 30u));
        }
        RequestQuality(false); Complete(true);
    }
    Initialize(Old3DS); Complete(true); RequestQuality(false); assert(!Pending());
    Initialize(Old3DS); RequestQuality(true); AllowChanges(false);
    assert(Pending() && !CanApply()); AllowChanges(true); assert(CanApply());
    Complete(false); assert(!Quality() && RenderWidth() == 400);
    assert(!strcmp(Status(), "QUALITY UNAVAILABLE"));
    Initialize(Old3DS); assert(!RequestedQuality());
    Initialize(Old2DS); RequestQuality(true); Complete(true);
    Initialize(New3DS); assert(!RequestedQuality());
    Initialize(New3DSXL); assert(!RequestedQuality());
    char executable[]="test", stereo[]="graphics_stereo=1", quality[]="graphics_quality=1";
    char* diagnostic[]={executable, stereo, quality};
    Initialize(Old3DS, 3, diagnostic); assert(Stereo() && !RequestedQuality()); Complete(true);
    RequestQuality(true); assert(!Pending() && !RequestedQuality());
    Initialize(Old2DS, 3, diagnostic); assert(!Stereo() && RequestedQuality());
    Initialize(New2DSXL, 3, diagnostic); assert(!Stereo() && RequestedQuality());
    Initialize(255); assert(SystemModel() == Unknown && !SupportsWide() && !RequestedQuality());
    PortSettings::SetNoTimers(true); PortSettings::Save();
}
'''
with tempfile.TemporaryDirectory(prefix="srr-graphics-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(source)
    binary = path / "test"
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-I", str(root / "code"),
        str(path / "test.cpp"), str(root / "code/main/graphics3ds.cpp"),
        str(root / "code/cheats/portsettings.cpp"), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], cwd=path, check=True)
    subprocess.run([str(binary), "read"], cwd=path, check=True)
print("PASS: six models, unknown fallback, mode defaults, isolated persistent preferences, failed activation and stereo exclusions.")
