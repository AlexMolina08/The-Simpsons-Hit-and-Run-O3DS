#!/usr/bin/env python3
"""Check the actual HUD lookup seam, including late assets and level replacement."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "code/presentation/gui/utility/hudmap.cpp").read_text()
a = source.index("    tSprite* BoardBackground()")
b = source.index("    void Board(", a)
method = source[a:b]
code = r'''
#include <cassert>
#include <cstddef>
struct tSprite{int id;};
namespace Scrooby {
struct Sprite{const tSprite* raw=nullptr;const tSprite* GetRawSprite(){return raw;}};
struct Page{Sprite* sprite=nullptr;Sprite* GetSprite(const char*){return sprite;}};
struct Screen{Page* page=nullptr;Page* GetPage(const char*){return page;}};
}
struct CGuiScreenHud{Scrooby::Screen screen;Scrooby::Screen* GetScroobyScreen(){return &screen;}};
enum ContextEnum{CONTEXT_GAMEPLAY,CONTEXT_PAUSE,CONTEXT_FRONTEND};
struct Flow{ContextEnum context=CONTEXT_GAMEPLAY;ContextEnum GetCurrentContext(){return context;}} flow;
Flow* GetGameFlow(){return &flow;}
CGuiScreenHud* currentHud=nullptr;
CGuiScreenHud* GetCurrentHud(){return currentHud;}
tSprite* globalSprite=nullptr;unsigned globalSearches=0;
tSprite* UIAsset(const char*){++globalSearches;return globalSprite;}
METHOD
int main(){
 tSprite first{1},second{2},global{3};CGuiScreenHud hud;
 Scrooby::Sprite art;art.raw=&first;Scrooby::Page page;page.sprite=&art;
 hud.screen.page=&page;currentHud=&hud;
 assert(BoardBackground()==&first && globalSearches==1);
 art.raw=&second;assert(BoardBackground()==&second && globalSearches==2);
 flow.context=CONTEXT_PAUSE;assert(BoardBackground()==&second && globalSearches==3);
 art.raw=nullptr;globalSprite=&global;assert(BoardBackground()==&global);
 globalSprite=nullptr;assert(BoardBackground()==nullptr);
 art.raw=&second;flow.context=CONTEXT_FRONTEND;globalSprite=&global;
 assert(BoardBackground()==&global);
 flow.context=CONTEXT_GAMEPLAY;currentHud=nullptr;assert(BoardBackground()==&global);
 globalSprite=nullptr;assert(BoardBackground()==nullptr);
 currentHud=&hud;hud.screen.page=nullptr;assert(BoardBackground()==nullptr);
 hud.screen.page=&page;assert(BoardBackground()==&second);
}
'''.replace("METHOD", method)
with tempfile.TemporaryDirectory(prefix="srr-hud-background-") as directory:
    p = Path(directory)
    (p / "test.cpp").write_text(code)
    subprocess.run(["clang++", "-std=c++14", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-DSRR2_OLD3DS_BOTTOM_MAP_15HZ",
                    "-DSRR2_OLD3DS_NATIVE_BENCH", str(p / "test.cpp"), "-o", str(p / "test")], check=True)
    subprocess.run([str(p / "test")], check=True)
print("PASS: production background lookup observes late and replaced HUD assets, pause/menu fallback and absent images without retaining stale pointers.")
