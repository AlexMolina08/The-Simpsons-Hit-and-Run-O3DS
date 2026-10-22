#!/usr/bin/env python3
"""Exercise the production Graphics/Cheats intro through repeated page changes."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "code/presentation/gui/frontend/guiscreencheats.cpp").read_text()
start = source.index("void CGuiScreenCheats::InitIntro()")
intro = source[start:source.index("\nvoid CGuiScreenCheats::InitRunning", start)]
fixture = r'''
#include <cassert>
enum {MENU_ITEM_NO_TIMERS, MENU_ITEM_CHEAT, MENU_ITEM_APPLY, MENU_ITEM_QUALITY,
      NUM_MENU_ITEMS, BUTTON_ICON_ACCEPT};
bool s_openGraphicsPage=false;
namespace Graphics3DS {
 enum {Old3DS,Unknown};
 int model=Old3DS; bool stereo=false,quality=false;
 int SystemModel(){return model;} bool Stereo(){return stereo;}
 bool RequestedQuality(){return quality;}
}
namespace PortSettings {bool NoTimers(){return false;}}
struct Text {bool visible=true;void SetVisible(bool on){visible=on;}};
struct Item {Text label,value;bool enabled=true;Text* GetItem(){return &label;}};
struct Menu {
 Item items[NUM_MENU_ITEMS];int selection=0,values[NUM_MENU_ITEMS]={};
 int GetNumItems(){return NUM_MENU_ITEMS;}
 void SetMenuItemEnabled(int i,bool enabled,bool changeVisibility){
  items[i].enabled=enabled;
  if(changeVisibility){items[i].label.SetVisible(enabled);items[i].value.SetVisible(enabled);}
 }
 Item* GetMenuItem(int i){return &items[i];}
 void Reset(int i){selection=i;}int GetSelection(){return selection;}
 void SetSelectionValue(int i,int value){values[i]=value;}
};
struct CGuiScreenCheats {
 Menu* m_pMenu;bool m_graphicsPage=false,accept=false;
 Text* m_qualityValue;unsigned refreshes=0;
 void InitIntro();void RefreshCheatLabels(){++refreshes;}
 void SetButtonVisible(int,bool visible){accept=visible;}
};
'''
main = r'''
int main(){
 Menu menu;CGuiScreenCheats screen;screen.m_pMenu=&menu;
 screen.m_qualityValue=&menu.items[MENU_ITEM_QUALITY].value;
 for(unsigned cycle=0;cycle<4;++cycle){
  for(bool graphics:{true,false}){
   s_openGraphicsPage=graphics;Graphics3DS::quality=cycle%2;
   screen.InitIntro();assert(screen.m_graphicsPage==graphics);
   for(int i=MENU_ITEM_NO_TIMERS;i<=MENU_ITEM_APPLY;++i){
    assert(menu.items[i].enabled==!graphics);
    assert(menu.items[i].label.visible==!graphics);
    assert(menu.items[i].value.visible==!graphics);
   }
   assert(menu.items[MENU_ITEM_QUALITY].label.visible==graphics);
   assert(menu.items[MENU_ITEM_QUALITY].value.visible==graphics);
   assert(menu.values[MENU_ITEM_QUALITY]==int(Graphics3DS::quality));
   assert(menu.selection==(graphics?MENU_ITEM_QUALITY:MENU_ITEM_NO_TIMERS));
  }
 }
 s_openGraphicsPage=true;Graphics3DS::stereo=true;screen.InitIntro();
 assert(!menu.items[MENU_ITEM_QUALITY].enabled&&screen.m_qualityValue->visible);
 Graphics3DS::stereo=false;Graphics3DS::model=Graphics3DS::Unknown;screen.InitIntro();
 assert(!menu.items[MENU_ITEM_QUALITY].enabled);
 assert(screen.refreshes==10);
}
'''
with tempfile.TemporaryDirectory(prefix="srr-graphics-menu-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text("#include <initializer_list>\n" + fixture + intro + main)
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", str(path / "test.cpp"), "-o", str(path / "test"),
    ], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PASS: repeated Graphics/Cheats navigation restores every row and respects stereo/unknown-model restrictions.")
