#!/usr/bin/env python3
"""Exercise live slider policy, restoration and preference isolation across runs."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = r'''
#include <main/graphics3ds.h>
#include <cheats/portsettings.h>
#include <cassert>
#include <cstring>
#include <initializer_list>
using namespace Graphics3DS;
int main(int argc,char** argv) {
    if(argc>1 && !strcmp(argv[1],"read")) {
        Initialize(Old3DS);assert(RequestedQuality());
        Initialize(Old2DS);assert(RequestedQuality());
        Initialize(New3DS);assert(!RequestedQuality());
        assert(PortSettings::NoTimers());return 0;
    }
    PortSettings::SetNoTimers(true);
    PortSettings::SetGraphicsQuality(0,true);
    PortSettings::SetGraphicsQuality(1,true);
    PortSettings::SetGraphicsQuality(2,false);
    PortSettings::Save();
    for(int m : {Old3DS,Old3DSXL,New3DS,New3DSXL}) {
        Initialize(m);Complete(true);const bool quality=Quality();
        AllowChanges(false);
        for(unsigned cycle=0;cycle<32;++cycle) {
            UpdateSlider(.25f);
            assert(CanApply() && Stereo() && !RequestedQuality());Complete(true);
            assert(!Quality() && RenderWidth()==400);
            assert(TargetFPS()==(IsNew()?60u:30u));
            RequestQuality(true);assert(!Pending() && !RequestedQuality());
            UpdateSlider(.5f);assert(!Pending());
            UpdateSlider(1);assert(!Pending());
            UpdateSlider(0);assert(CanApply() && !Stereo());Complete(true);
            assert(Quality()==quality && RenderWidth()==(quality?800u:400u));
            UpdateSlider(StereoDepth::SliderDeadzone);assert(!Pending());
        }
    }
    for(int m : {Old2DS,New2DSXL,Unknown}) {
        Initialize(m);Complete(true);bool quality=Quality();
        UpdateSlider(1);assert(!Stereo() && !Pending() && quality==Quality());
    }
    Initialize(Old3DS);Complete(true);UpdateSlider(1);Complete(false);
    assert(!Stereo() && !Quality());
    UpdateSlider(1);assert(!Pending()); // Do not retry a failed allocation each frame.
    UpdateSlider(0);assert(CanApply() && RequestedQuality());Complete(true);assert(Quality());
    UpdateSlider(1);Complete(true);UpdateSlider(0);Complete(true);
    RequestQuality(false);Complete(true);UpdateSlider(1);Complete(true);
    UpdateSlider(0);Complete(true);assert(!Quality());
    RequestQuality(true);Complete(true);
    char app[]="test",off[]="graphics_stereo=0",on[]="graphics_stereo=1";
    char* disabled[]={app,off};char* enabled[]={app,on};
    Initialize(Old3DS,2,disabled);Complete(true);UpdateSlider(1);assert(!Stereo() && !Pending());
    Initialize(Old3DS,2,enabled);Complete(true);UpdateSlider(0);assert(Stereo() && !Pending());
}
'''
with tempfile.TemporaryDirectory(prefix="srr-stereo-slider-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(source)
    binary = path / "test"
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-I", str(root / "code"),
        str(path / "test.cpp"), str(root / "code/main/graphics3ds.cpp"),
        str(root / "code/cheats/portsettings.cpp"), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], cwd=path, check=True, stdout=subprocess.DEVNULL)
    settings = (path / "srr2_settings.ini").read_text()
    assert "stereo" not in settings
    subprocess.run([str(binary), "read"], cwd=path, check=True)
print("PASS: live slider cycles, original/New/2DS policies, Quality restoration, failure retry and unchanged saved preferences.")
