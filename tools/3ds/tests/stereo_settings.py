#!/usr/bin/env python3
"""Check saved stereo choices across launches without losing Quality or cheats."""
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
    assert(argc == 2);
    if(!strcmp(argv[1], "enable")) {
        for(int m=Old3DS; m<=New2DSXL; ++m) {
            Initialize(m); assert(!Stereo() && !RequestedStereo());
        }
        PortSettings::SetNoTimers(true);
        PortSettings::SetGraphicsQuality(0,true);
        PortSettings::SetGraphicsQuality(1,false);
        PortSettings::SetGraphicsQuality(2,true);
        Initialize(Old3DS); Complete(true);
        assert(Quality() && RenderWidth()==800);
        RequestStereo(true);
        assert(RequestedStereo() && !Stereo() && !Pending());
        assert(Quality() && RenderWidth()==800);
        assert(!strcmp(Status(),"RESTART GAME TO APPLY 3D"));
        RequestQuality(false); assert(RequestedQuality());
        RequestStereo(false);
        assert(!RequestedStereo() && !Stereo() && Quality());
        assert(!strcmp(Status(),"800x240 / 30 FPS TARGET"));
        RequestStereo(true);
        Initialize(New3DS); Complete(true); RequestStereo(true);
        assert(RequestedStereo() && !Stereo() && Quality());
    } else if(!strcmp(argv[1], "loaded")) {
        for(int m : {Old3DS,Old3DSXL,New3DS,New3DSXL}) {
            Initialize(m);
            assert(Stereo() && RequestedStereo() && !RequestedQuality());
            Complete(true); assert(!Quality() && RenderWidth()==400);
            assert(TargetFPS()==(IsNew()?60u:30u));
            RequestQuality(true); assert(!Pending() && !RequestedQuality());
        }
        Initialize(Old2DS); Complete(true);
        assert(!Stereo() && !RequestedStereo() && !Quality());
        RequestStereo(true); assert(!RequestedStereo());
        Initialize(New2DSXL); Complete(true);
        assert(!Stereo() && !RequestedStereo() && Quality());
        RequestStereo(false); assert(PortSettings::Stereo3D(2));
        Initialize(Unknown); RequestStereo(true); assert(!Stereo() && !RequestedStereo());
        assert(PortSettings::NoTimers());
    } else if(!strcmp(argv[1], "disable")) {
        for(int m : {Old3DS,New3DS}) {
            Initialize(m); Complete(true); RequestStereo(false);
            assert(!RequestedStereo() && Stereo() && RenderWidth()==400 && !Pending());
            assert(!strcmp(Status(),"RESTART GAME TO APPLY 3D"));
            RequestQuality(true); assert(!RequestedQuality());
            assert(PortSettings::GraphicsQuality(IsNew()?2:0,false));
        }
    } else if(!strcmp(argv[1], "restored")) {
        for(int m : {Old3DS,New3DS}) {
            Initialize(m); assert(!Stereo() && !RequestedStereo() && RequestedQuality());
            Complete(true); assert(Quality() && RenderWidth()==800);
        }
        Initialize(Old2DS); assert(!RequestedQuality());
        assert(PortSettings::NoTimers());
    } else if(!strcmp(argv[1], "override")) {
        char app[]="test",on[]="graphics_stereo=1",off[]="graphics_stereo=0";
        char* enabled[]={app,on}; char* disabled[]={app,off};
        Initialize(Old3DS,2,enabled); assert(Stereo());
        Initialize(Old3DS,2,disabled); assert(!Stereo());
        Initialize(Old3DS); assert(!Stereo());
        assert(!PortSettings::Stereo3D(0));
    } else {
        assert(false);
    }
}
'''
with tempfile.TemporaryDirectory(prefix="srr-stereo-settings-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text("#include <initializer_list>\n" + source)
    binary = path / "test"
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-I", str(root / "code"),
        str(path / "test.cpp"), str(root / "code/main/graphics3ds.cpp"),
        str(root / "code/cheats/portsettings.cpp"), "-o", str(binary),
    ], check=True)
    settings = path / "srr2_settings.ini"
    for action in ("enable", "loaded", "disable", "restored", "override"):
        before = settings.read_bytes() if settings.exists() else None
        subprocess.run([str(binary), action], cwd=path, check=True)
        if action in ("loaded", "restored", "override"):
            assert settings.read_bytes() == before, "Read-only launch changed saved settings"
    with settings.open("a") as file:
        file.write("stereo_3d_0=2\nstereo_3d_0=-1\nstereo_3d_3=1\nstereo_3d_-1=1\n")
    subprocess.run([str(binary), "restored"], cwd=path, check=True)
print("PASS: stereo persistence, restart-only activation, model exclusions, Quality restoration, cheats and temporary overrides.")
