#!/usr/bin/env python3
"""Exercise production breadcrumb gates before using diagnostic SD writes."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3];source=(root/'code/main/nativebench.cpp').read_text()
start=source.index('static void Checkpoint(const char* boundary,int stage)');end=source.index('void Add(Counter c',start)
fixture=r'''
#include <cstdio>
#include <cassert>
static bool checkpointIO=false,collecting=false,available=false;
static unsigned frame=0,elapsedMs=0,g_picaPresentedFrames=0,loads=0,linearReads=0;
enum Stage {Update,Render,Stream,Service,Count};
static const char* names[Count]={"update","render","stream","service"};
struct Manager {unsigned GetNumCurrentRequests(){assert(available);++loads;return 3;}} manager;
Manager* GetLoadingManager(){assert(available);return &manager;}
unsigned linearSpaceFree(){assert(available);++linearReads;return 12345;}
'''
main=r'''
int main(){
 Checkpoint("enter",-1);frame=16;collecting=true;Checkpoint("enter",Update);assert(loads==0);
 checkpointIO=true;frame=0;Checkpoint("enter",Update);assert(loads==0);
 frame=1;Checkpoint("enter",Update);assert(loads==0);
 frame=16;collecting=false;Checkpoint("enter",Update);assert(loads==0);
 collecting=available=true;elapsedMs=777;g_picaPresentedFrames=99;
 Checkpoint("enter",-1);Checkpoint("leave",Service);assert(loads==2 && linearReads==2);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(fixture+source[start:end]+main)
 subprocess.run(['c++','-std=c++17','-O2',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 output=subprocess.check_output([str(p/'test')],text=True).splitlines()
 assert len(output)==2 and all('performance_capture=0' in line and 'frame=16 route_ms=777' in line for line in output)
 assert 'boundary=enter stage=frame' in output[0] and 'boundary=leave stage=service' in output[1]
print('PASS: disabled, pre-singleton calibration, unselected and noncapture paths perform no reads/I/O; selected boundaries retain identity and exclusion flags.')
