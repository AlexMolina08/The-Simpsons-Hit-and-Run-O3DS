// Native touch geometry must agree with the rendered controls at every pixel.
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "libs/pure3d/pddi/pica/bottomframe.hpp"
#include "libs/radcore/src/radcontroller/old3dsinput.hpp"

int main()
{
    Old3DSInput::TouchActionPulse action;
    assert(!action.Value(0));
    action.Request(100);
    for(unsigned now=100;now<250;++now)assert(action.Value(now));
    assert(!action.Value(250) && !action.Value(251));
    action.Request(300);action.Request(400);assert(action.Value(549));
    action.Cancel();assert(!action.Value(550));
    action.Request(0xfffffff0u);assert(action.Value(0x25));assert(!action.Value(0x90));
    using namespace picaBottomFrame;
    assert(Radar.top>=38 && Radar.bottom<=Objective.top-6);
    assert(Radar.left>=Interaction.right+8 && Radar.right<=320);
    assert(13>=Objective.left && 53<=Objective.right && 190>=Objective.top && 230<=Objective.bottom);
    assert(IdleRadar.left>=Interaction.right+6 && IdleRadar.top>=38 && IdleRadar.bottom<=210);
    for(int count : {5,6}){
        const int order[6]={5,4,0,1,2,3};
        for(int row=0;row<count;++row){
            assert(MainChoice(row,count)==order[row+(count==5?1:0)]);
            assert(MainRow(MainChoice(row,count),count)==row);
        }
        assert(MainChoice(count-1,count)==3); // Bonus Game remains last.
        for(float position : {0.0f,0.25f,0.5f,0.75f,1.0f})
            for(int y=0;y<240;++y)for(int x=0;x<320;++x){
                int expected=-1;
                if(x>=44 && x<276 && y>=58 && y<188)
                    for(int row=0;row<count;++row)if(MainItem(row,position).Contains(x,y))expected=row;
                assert(MainHit(x,y,count,position)==expected);
            }
    }
    MenuScroll scroll;scroll.Follow(5,6,100);
    assert(scroll.Value(100)==0 && scroll.Value(160)==0.5f && scroll.Value(220)==1);
    scroll.Follow(0,6,220);assert(scroll.Value(280)==0.5f && scroll.Value(340)==0);
    scroll.Drag(0.4f,6,400);assert(scroll.Value(400)==0.4f);
    scroll.Move(1,6,400);const float interrupted=scroll.Value(460);
    scroll.Move(0,6,460);assert(scroll.Value(460)==interrupted && scroll.Value(580)==0);
    scroll.Drag(-20,6,600);assert(scroll.Value(600)==0);
    scroll.Drag(20,6,600);assert(scroll.Value(600)==1);
    scroll.Drag(20,5,600);assert(scroll.Value(600)==0);
    scroll.Follow(5,6,0xfffffff0u);assert(scroll.Value(0x68)==1);
    TouchOwner owner;int screenA=0,screenB=0,menuA=0;
    assert(owner.Update(&screenA,&menuA) && owner.epoch==1);
    assert(!owner.Update(&screenA,&menuA) && owner.epoch==1);
    assert(owner.Update(&screenB,nullptr) && owner.epoch==2);
    assert(owner.Update(&screenA,&menuA) && owner.epoch==3);
    assert(owner.Update(&screenA,nullptr) && owner.epoch==4);
    MenuFade fade;assert(fade.Value(100)==0);
    fade.Set(true,100);assert(fade.Value(100)==0 && fade.Value(160)==0.5f && fade.Value(220)==1);
    fade.Set(false,220);assert(fade.Value(280)==0.5f && fade.Value(340)==0);
    fade.Set(true,400);fade.Set(false,460);assert(fade.Value(460)==0.5f && fade.Value(580)==0);
    fade.Set(true,0xfffffff0u);assert(fade.Value(0x68)==1);
    TouchEdges edges;
    edges.Update(false);assert(!edges.down && !edges.up);
    edges.Update(true);assert(edges.down && !edges.up);
    edges.Update(true);assert(!edges.down && !edges.up);
    edges.Update(false);assert(!edges.down && edges.up);
    edges.Update(false);assert(!edges.down && !edges.up);
    // Entering another screen while the pen is held must not create a second press.
    edges.Reset(true);edges.Update(true);assert(!edges.down && !edges.up);
    edges.Update(false);assert(edges.up && !edges.down);
    for(int count=2;count<=8;++count) {
        for(int item=0;item<count;++item) {
            const auto r=PauseItem(item,count);
            assert(r.left>=0 && r.right<=320 && r.top>=33 && r.bottom<=210);
            for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)
                assert(PauseHit(x,y,count)==item);
            for(int other=item+1;other<count;++other) {
                const auto s=PauseItem(other,count);
                assert(r.right<=s.left || s.right<=r.left || r.bottom<=s.top || s.bottom<=r.top);
            }
            for(int direction=-1;direction<=1;++direction) {
                const int horizontal=PauseMove(item,count,direction,0);
                const int vertical=PauseMove(item,count,0,direction);
                assert(horizontal>=0 && horizontal<count && vertical>=0 && vertical<count);
            }
        }
        for(int y=212;y<240;++y)for(int x=0;x<320;++x)assert(PauseHit(x,y,count)==-1);
    }
    for(int count=1;count<=6;++count) {
        const int top=MenuTop(count);
        assert(top>=33 && top+count*MenuStep(count)<=210);
        for(int row=0;row<count;++row){
            const auto rect=MenuItem(row,count);
            for(int y=rect.top;y<rect.bottom;++y)for(int x=rect.left;x<rect.right;++x)
                assert(MenuHit(x,y,count)==row);
            assert(MenuHit(160,rect.bottom,count)==-1);
        }
        assert(MenuHit(0,top,count)==-1 && MenuHit(319,top,count)==-1);
    }
    for(int count=1;count<=8;++count)for(int item=0;item<count;++item) {
        const auto r=Thumbnail(item);
        assert(r.left>=0 && r.right<=320 && r.top>=33 && r.bottom<=210);
        for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)
            assert(ThumbnailHit(x,y,count)==item);
    }
    for(int count=1;count<=TVPageSize;++count)for(int item=0;item<count;++item){
        const auto r=TVMenuItem(item,count);
        assert(r.left>TVGlass.left && r.right<TVGlass.right && r.top>=55 && r.bottom+3<=190);
        for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)assert(TVMenuHit(x,y,count)==item);
        assert(TVMenuHit(r.left-1,r.top,count)==-1 && TVMenuHit(r.right,r.top,count)==-1);
        assert(TVMenuHit(r.left,r.bottom,count)==-1);
        for(int y=190;y<240;++y)assert(TVMenuHit(160,y,count)==-1);
    }
    for(int count=1;count<=8;++count)for(int item=0;item<count;++item){
        const auto r=TVThumbnail(item);
        assert(r.left>TVGlass.left && r.right<TVGlass.right && r.top>=55 && r.bottom+3<=190);
        for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)assert(TVThumbnailHit(x,y,count)==item);
    }
    for(bool tv:{false,true}){
        const int left=tv?194:202,right=tv?266:298;
        for(int x=12;x<150;++x)assert(!SliderTrackHit(x,tv));
        assert(SliderTrackHit(left,tv) && SliderTrackHit(right,tv));
        assert(SliderValue(left,tv)==0 && SliderValue(right,tv)==1);
        assert(SliderValue((left+right)/2,tv)==0.5f);
        assert(SliderValue(-1,tv)==0 && SliderValue(320,tv)==1);
    }
    puts("PASS: native touch targets, gutters, footer separation, grid navigation, main-menu order, scrolling and animated hit testing.");
}
