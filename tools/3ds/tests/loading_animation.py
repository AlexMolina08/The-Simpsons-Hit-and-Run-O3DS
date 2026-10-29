#!/usr/bin/env python3
"""Exercise the production newspaper entrance, asynchronous start and loading slice."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]

def method(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

source = (root / 'code/presentation/gui/backend/guiscreenloading.cpp').read_text()
hook_start = source.index('#ifdef RAD_3DS\nnamespace { bool s_newspaperEntranceActive=false; }')
hook_end = source.index('#endif', hook_start) + len('#endif')
hook = source[hook_start:hook_end]
start = source.index('                static float SPIRAL_DURATION_TIME')
animation = source[start:source.index('\n            }', start)]
start = source.index('    m_loadingImage->SetRawSprite(')
ready = source[start:source.index('\n#endif\n}', start)]
game = (root / 'code/main/game.cpp').read_text()
start = game.index('        g_p3dYieldTimeUs = ctx == CONTEXT_GAMEPLAY')
assignment = game[start:game.index(';', start) + 1]
spiral = method((root / 'code/presentation/gui/utility/specialfx.cpp').read_text(), 'bool GuiSFX::Spiral')
font_getter = method((root / 'libs/scrooby/src/FeText.cpp').read_text(), 'tFont* FeText::GetFont() const')
program = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#define SRR2_OLD_3DS
#define rAssert assert
static bool s_newspaperEntranceActive=false;
bool old3dsNewspaperAnimating() { return s_newspaperEntranceActive; }
namespace rmt {
constexpr float PI_2=6.28318530717958647692f;
float RadianToDeg(float value) { return value * (180.0f / 3.14159265358979323846f); }
}
namespace Scrooby { struct BoundedDrawable {
    float scale=0, angle=0, alpha=0;
    bool visible=false;
    void ResetTransformation() { scale=angle=0; }
    void ScaleAboutCenter(float value) { scale=value; }
    void RotateAboutCenter(float value) { angle=value; }
    void SetAlpha(float value) { alpha=value; }
    void SetVisible(bool value) { visible=value; }
    void SetRawSprite(void*, bool) {}
}; }
namespace GuiSFX { bool Spiral(Scrooby::BoundedDrawable*,float,float,float,float,float,bool=false); }
''' + spiral + r'''
struct Entrance {
    unsigned m_elapsedSpiralTime=0;
    bool m_isSpirallingDone=false;
    Scrooby::BoundedDrawable image;
    Scrooby::BoundedDrawable* m_loadingImage=&image;
    void* m_loadingImageSprite=nullptr;
    static constexpr float LOADING_IMAGE_CORRECTION_SCALE=1.0f;
    void Ready() {
''' + ready + r'''
    }
    void Update(unsigned param1) {
''' + animation + r'''
    }
};
enum { CONTEXT_GAMEPLAY, CONTEXT_PAUSE, CONTEXT_LOADING_GAMEPLAY,
       CONTEXT_FRONTEND, CONTEXT_LOADING_FRONTEND };
unsigned budget(int ctx) {
    unsigned g_p3dYieldTimeUs=0;
''' + assignment + r'''
    return g_p3dYieldTimeUs;
}
struct Resource { virtual ~Resource()=default; };
struct tFont:Resource { unsigned references=0; void AddRef() { ++references; } };
struct Resources { Resource* font=nullptr; Resource* GetResource(unsigned) { return font; } } resources;
struct FeApp {
    static FeApp* GetInstance() { static FeApp app; return &app; }
    Resources& GetFeResourceManager() { return resources; }
};
struct FeText { tFont* mFont=nullptr; unsigned mTextStyle=1; tFont* GetFont() const; };
''' + font_getter + r'''
int main() {
    for(unsigned dt : {16u,33u,34u,50u,67u}) {
        Entrance entrance;
        // Image loading can outlast the entire animation. Readiness must restart it.
        entrance.Update(5000);
        assert(entrance.m_isSpirallingDone);
        entrance.Ready();
        assert(!entrance.m_isSpirallingDone && entrance.m_elapsedSpiralTime==0);
        assert(entrance.image.visible && entrance.image.scale==0);
        float angle=0, scale=.25f;
        unsigned rendered=0;
        for(unsigned t=0; t<1900; t+=dt) {
            entrance.Update(dt);
            assert(std::isfinite(entrance.image.angle));
            const float angularStep=std::remainder(entrance.image.angle-angle,360.0f);
            assert(angularStep>=-.01f && angularStep<90);
            assert(entrance.image.scale>=scale && entrance.image.scale<=1.00001f);
            assert(entrance.m_isSpirallingDone==(entrance.m_elapsedSpiralTime>1800));
            if(!entrance.m_isSpirallingDone) ++rendered;
            angle=entrance.image.angle; scale=entrance.image.scale;
        }
        assert(rendered>=26 && angle==0 && scale==1);
    }
    Entrance end;
    end.Update(1800);
    assert(std::abs(end.image.angle-1080)<.001f && end.image.scale==1);
    end.Update(1);
    assert(end.m_isSpirallingDone && end.image.angle==0);
    s_newspaperEntranceActive=true;
    assert(budget(CONTEXT_LOADING_GAMEPLAY)<=8000);
    s_newspaperEntranceActive=false;
    assert(budget(CONTEXT_LOADING_GAMEPLAY)==52000);
    assert(budget(CONTEXT_GAMEPLAY)==2000 && budget(CONTEXT_PAUSE)==8000);
    assert(budget(CONTEXT_FRONTEND)==52000 && budget(CONTEXT_LOADING_FRONTEND)==52000);
    tFont atlas;
    FeText label;
    assert(!label.GetFont());
    resources.font=&atlas;
    assert(label.GetFont()==&atlas && atlas.references==1);
    assert(label.GetFont()==&atlas && atlas.references==1);
    puts("PASS: original three-turn spiral at 15–60 FPS; delayed image restarts entrance; upright landing; loading budget; lazy font acquisition without duplicate references.");
}
'''
with tempfile.TemporaryDirectory(prefix='srr-loading-animation-') as temporary:
    path = Path(temporary)
    (path / 'test.cpp').write_text(program)
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
    # The ordinary 3DS build also calls this hook, with the entrance flag off.
    (path / 'standard.cpp').write_text('#define RAD_3DS\n' + hook +
                                     '\nint main() { return old3dsNewspaperAnimating() ? 1 : 0; }\n')
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    str(path / 'standard.cpp'), '-o', str(path / 'standard')], check=True)
    subprocess.run([str(path / 'standard')], check=True)
    print('PASS: loading hook links in the standard 3DS configuration.')
