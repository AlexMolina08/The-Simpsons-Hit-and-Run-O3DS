#!/usr/bin/env python3
"""Render the real briefing footer while loading artwork repeatedly blinks."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "code/presentation/gui/utility/hudmap.cpp").read_text()
start = source.index("    void BriefingPanel(CGuiScreen* screen)")
panel = source[start:source.index("    // BeginRender replaces", start)]
fixture = r'''
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
struct tFont {};
namespace Scrooby {
 struct Screen {};
 struct Text {
  bool visible=true;const char* text="";tFont font;
  bool IsVisible(){return visible;}tFont* GetFont(){return &font;}
  const char* GetStringBuffer(){return text;}
 } title,info,status;
}
struct CGuiWindow {enum {GUI_SCREEN_ID_MISSION_LOAD,GUI_SCREEN_ID_MISSION_SUCCESS};};
enum {BUTTON_ICON_ACCEPT,BUTTON_ICON_BACK,Gold,White};
struct CGuiScreen {
 int id=CGuiWindow::GUI_SCREEN_ID_MISSION_LOAD;Scrooby::Screen resource;
 Scrooby::Screen* GetScroobyScreen(){return &resource;}
 int GetWindowID(){return id;}bool IsButtonVisible(int){return true;}
};
struct CGuiScreenMissionLoad : CGuiScreen {
 bool done=false;bool IsLoadDone() const {return done;}
};
bool missingStatus=false;
Scrooby::Text* FindText(Scrooby::Screen*,const char* name){
 if(!strcmp(name,"Loading"))return missingStatus?nullptr:&Scrooby::status;
 return !strcmp(name,"MissionTitle")?&Scrooby::title:&Scrooby::info;
}
std::vector<std::string> output;
void BottomLabel(tFont*,const char* text,int,int,int,int,int){output.push_back(text);}
void Wrapped(tFont*,const char*,int,int,int,int,int,int){}
void Ascii(tFont*,const char* text,int,int,int,int,int){output.push_back(text);}
void FooterAction(tFont*,const char*,const char* text,int,int){output.push_back(text);}
bool contains(const char* text){for(const auto& value:output)if(value==text)return true;return false;}
'''
main = r'''
int main(){
 CGuiScreenMissionLoad screen;Scrooby::status.text="LOADING";
 for(unsigned mission=0;mission<3;++mission){
  screen.done=false;
  for(unsigned frame=0;frame<20;++frame){
   Scrooby::status.visible=frame%2;output.clear();BriefingPanel(&screen);
   assert(contains("LOADING")&&!contains("START MISSION")&&!contains("CANCEL"));
  }
  missingStatus=true;output.clear();BriefingPanel(&screen);assert(contains("LOADING"));missingStatus=false;
  screen.done=true;Scrooby::status.visible=false;output.clear();BriefingPanel(&screen);
  assert(!contains("LOADING")&&contains("START MISSION")&&contains("CANCEL"));
 }
 CGuiScreen success;success.id=CGuiWindow::GUI_SCREEN_ID_MISSION_SUCCESS;
 output.clear();BriefingPanel(&success);assert(contains("CONTINUE")&&!contains("START MISSION"));
}
'''
with tempfile.TemporaryDirectory(prefix="srr-briefing-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(fixture + panel + main)
    subprocess.run([
        "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", str(path / "test.cpp"), "-o", str(path / "test"),
    ], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PASS: blinking artwork cannot show Start Mission before loading completes; ready, repeat and success states remain correct.")
