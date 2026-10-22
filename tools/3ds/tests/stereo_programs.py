#!/usr/bin/env python3
"""Exercise actual program loading and live mono/stereo binding without reloads."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "libs/pure3d/pddi/pica/context.cpp").read_text()
start = source.index("static void LoadStereoProgram(")
end = source.index("#ifdef SRR2_OLD3DS_UNLIT_PROJECTION", start)
methods = source[start:end]
# The loader is guarded separately in production; include the matching opener.
methods = "#ifdef SRR2_3DS_STEREO_GPU_SKIN\n" + methods
fixture = r'''
#include <cassert>
using u8=unsigned char;using u32=unsigned;
#define SRR2_3DS_STEREO_GPU_SKIN
#define SRR2_OLD3DS_GPU_SKIN
#define SRR2_OLD3DS_NATIVE_CONTEXT
#define R_FAILED(value) ((value)<0)
struct DVLB_s {unsigned DVLE[1];} binaries[8];
struct shaderProgram_s {bool initialized=false;};
bool parseFails=false,programFails=false;unsigned parses=0,frees=0,programFrees=0;
DVLB_s* DVLB_ParseFile(u32*,unsigned){++parses;return parseFails?nullptr:&binaries[parses%8];}
void DVLB_Free(DVLB_s*){++frees;}
void shaderProgramInit(shaderProgram_s* p){p->initialized=true;}
int shaderProgramSetVsh(shaderProgram_s* p,unsigned*){assert(p->initialized);return programFails?-1:0;}
void shaderProgramFree(shaderProgram_s*){++programFrees;}
namespace Graphics3DS {bool stereo=false;bool Stereo(){return stereo;}}
shaderProgram_s *bound=nullptr,*registeredMain=nullptr,*registeredSkin=nullptr;
void C3D_BindProgram(shaderProgram_s* p){bound=p;}
void picaNativeRegisterPrograms(shaderProgram_s* main,shaderProgram_s* skin){registeredMain=main;registeredSkin=skin;}
struct picaContext {
 DVLB_s *skinBinary=&binaries[0],*stereoShaderBinary=nullptr,*stereoSkinBinary=nullptr;
 shaderProgram_s shaderProgram,skinProgram,stereoShaderProgram,stereoSkinProgram;
 bool skinProgramBound=true;
 shaderProgram_s* MainProgram();shaderProgram_s* SkinProgram();
 bool StereoShadersReady() const;void PrepareFramePrograms();
};
'''
main = r'''
int main(){
 const u8 data[4]={};picaContext c;
 assert(!c.StereoShadersReady());
 parseFails=true;LoadStereoProgram(c.stereoShaderBinary,c.stereoShaderProgram,data,4);
 assert(!c.stereoShaderBinary&&!frees&&!programFrees);
 parseFails=false;programFails=true;
 LoadStereoProgram(c.stereoShaderBinary,c.stereoShaderProgram,data,4);
 assert(!c.stereoShaderBinary&&frees==1&&programFrees==1);
 programFails=false;LoadStereoProgram(c.stereoShaderBinary,c.stereoShaderProgram,data,4);
 assert(!c.StereoShadersReady());
 LoadStereoProgram(c.stereoSkinBinary,c.stereoSkinProgram,data,4);assert(c.StereoShadersReady());
 const unsigned loaded=parses;
 for(unsigned cycle=0;cycle<64;++cycle){
  Graphics3DS::stereo=cycle%2;c.skinProgramBound=true;c.PrepareFramePrograms();
  assert(!c.skinProgramBound && bound==c.MainProgram());
  assert(bound==(Graphics3DS::stereo?&c.stereoShaderProgram:&c.shaderProgram));
  assert(registeredMain==bound && registeredSkin==c.SkinProgram());
  assert(registeredSkin==(Graphics3DS::stereo?&c.stereoSkinProgram:&c.skinProgram));
  assert(parses==loaded && frees==1 && programFrees==1);
 }
 c.stereoSkinBinary=nullptr;assert(!c.StereoShadersReady() && !c.SkinProgram());
}
'''
with tempfile.TemporaryDirectory(prefix="srr-stereo-programs-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(fixture + methods + main)
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", str(path / "test.cpp"), "-o", str(path / "test"),
    ], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PASS: distinct mono/stereo programs stay resident, frame bindings match native registration, and failed loads clean up.")
