#!/usr/bin/env python3
"""Exercise the production texture destructor across an unfinished GPU frame."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'libs/pure3d/pddi/pica/texture.cpp').read_text()
start = source.index('// Reserve the ownership node before creating GPU storage.')
end = source.index('\n#endif\n\n#ifdef SRR2_OLD3DS_VRAM_TEXTURES', start)
queue = source[start:end]
start = source.index('picaTexture::~picaTexture()')
end = source.index('\n#ifdef SRR2_OLD3DS_VISUAL_UPGRADE\nvoid picaTexture::SetWorldLightmapParameters', start)
destructor = source[start:end]

harness = r'''
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>
struct C3D_Tex { void* data; unsigned size; };
bool gpuComplete=false,failNode=false;
unsigned deleted=0;
void C3D_TexDelete(C3D_Tex* texture) {
    assert(gpuComplete);++deleted;std::free(texture->data);
}
void* PrepareNode(size_t size) {return failNode?nullptr:std::malloc(size);}
struct picaLinearTextureRetirement;
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
struct picaVramAllocation {void* data;};
std::vector<picaVramAllocation*> retiredVram;
void RetireVram(picaVramAllocation* node) {retiredVram.push_back(node);}
#endif
struct Context {void ADD_STAT(int,int){}} context;
enum {PDDI_STAT_TEXTURE_COUNT_32BIT};
struct picaTexture {
    bool created=true;C3D_Tex tex{};
    picaLinearTextureRetirement* linearRetirement=nullptr;
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    picaVramAllocation* vramAllocation=nullptr;
#endif
    Context* context=&::context;
    void FreeStaging(){}
    ~picaTexture();
};
#define malloc PrepareNode
'''
tests = r'''
#undef malloc
int main() {
    failNode=true;assert(!PrepareLinearTextureRetirement());failNode=false;
    std::vector<unsigned char*> queued;
    for(unsigned i=0;i<1000;++i) {
        auto* texture=new picaTexture;
        texture->linearRetirement=PrepareLinearTextureRetirement();
        assert(texture->linearRetirement);
        texture->tex={std::malloc(193),193};
        std::memset(texture->tex.data,0x69,193);
        queued.push_back(static_cast<unsigned char*>(texture->tex.data));
        delete texture;
    }
    assert(deleted==0);
    for(auto* bytes:queued)for(unsigned i=0;i<193;++i)assert(bytes[i]==0x69);
    gpuComplete=true;picaReclaimLinearTextures();
    assert(deleted==1000 && !retiredLinearTextures);
    picaReclaimLinearTextures();assert(deleted==1000);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    assert(linearTextureRetired==1000 && linearTextureReclaimed==1000);
    assert(linearTexturePending==0 && linearTexturePeak==1000);
#endif
    auto* empty=new picaTexture;empty->created=false;
    delete empty;assert(deleted==1000);
#ifdef SRR2_OLD3DS_VRAM_TEXTURES
    gpuComplete=false;
    auto* promoted=new picaTexture;
    promoted->linearRetirement=PrepareLinearTextureRetirement();
    promoted->tex={std::malloc(193),193};
    std::memset(promoted->tex.data,0x75,193);
    promoted->vramAllocation=new picaVramAllocation{promoted->tex.data};
    delete promoted;
    assert(deleted==1000 && !retiredLinearTextures && retiredVram.size()==1);
    auto* bytes=static_cast<unsigned char*>(retiredVram[0]->data);
    for(unsigned i=0;i<193;++i)assert(bytes[i]==0x75);
    gpuComplete=true;std::free(retiredVram[0]->data);delete retiredVram[0];
#endif
    puts("PASS: production texture destruction preserves queued bytes until fence; exact SDK frees, node failure and empty teardown");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-linear-texture-retire-') as temporary:
    folder = Path(temporary)
    (folder / 'test.cpp').write_text(harness + queue + '\n' + destructor + tests)
    for diagnostics, vram in ((False, False), (True, False), (False, True), (True, True)):
        flags = (['-DSRR2_OLD3DS_NATIVE_BENCH'] if diagnostics else []) + (['-DSRR2_OLD3DS_VRAM_TEXTURES'] if vram else [])
        subprocess.run(['clang++', '-std=c++14', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-DSRR2_OLD3DS_DRAW_PREP', *flags,
                        str(folder / 'test.cpp'), '-o', str(folder / 'test')], check=True)
        subprocess.run([str(folder / 'test')], check=True)
