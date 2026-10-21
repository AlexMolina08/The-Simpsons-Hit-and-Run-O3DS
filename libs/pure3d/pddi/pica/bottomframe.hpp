#ifndef PICA_BOTTOM_FRAME_HPP
#define PICA_BOTTOM_FRAME_HPP

namespace picaBottomFrame {
constexpr float Width=320, Height=240;
constexpr float Left=21, Top=18, Right=299, Bottom=221;
constexpr int MenuRowHeight=28;
inline int MenuStep(int count){return count<=4?40:count==5?32:MenuRowHeight;}
inline int MenuTop(int count){return 124-count*MenuStep(count)/2;}
struct TouchEdges {
    bool held=false,down=false,up=false;
    void Reset(bool currentHeld){held=currentHeld;down=up=false;}
    void Update(bool currentHeld){down=currentHeld&&!held;up=!currentHeld&&held;held=currentHeld;}
};
struct TouchOwner {
    const void* screen=nullptr;const void* menu=nullptr;unsigned epoch=0;
    bool Update(const void* nextScreen,const void* nextMenu){
        if(screen==nextScreen && menu==nextMenu)return false;
        screen=nextScreen;menu=nextMenu;++epoch;return true;
    }
};
struct MenuFade {
    float from=0,target=0;unsigned started=0;
    float Value(unsigned now) const {
        float t=(now-started)/120.0f;if(t>1)t=1;
        return from+(target-from)*t*t*(3-2*t);
    }
    void Set(bool active,unsigned now){const float next=active?1.0f:0.0f;
        if(next!=target){from=Value(now);target=next;started=now;}}
};
struct Rect {
    int left,top,right,bottom;
    bool Contains(int x,int y) const {return x>=left && x<right && y>=top && y<bottom;}
};
// Keep the radar crest and objective portrait inside separate native regions.
constexpr Rect Radar={94,44,226,176};
constexpr Rect IdleRadar={80,44,240,204};
constexpr Rect Objective={7,184,313,237};
constexpr Rect Interaction={6,118,74,182};
// Keep retail value IDs intact while presenting the handheld menu in use order.
inline int MainChoice(int row,int count){
    const int order[6]={5,4,0,1,2,3};
    return count==5 || count==6?order[row+(count==5?1:0)]:row;
}
inline int MainRow(int value,int count){for(int row=0;row<count;++row)if(MainChoice(row,count)==value)return row;return 0;}
struct MenuScroll {
    float from=0,target=0;unsigned started=0;
    float Value(unsigned now) const {float t=(now-started)/120.0f;if(t>1)t=1;return from+(target-from)*t*t*(3-2*t);}
    static float Clamp(float next,int count){const float maximum=count>5?count-5:0;return next<0?0:next>maximum?maximum:next;}
    void Move(float next,int count,unsigned now){next=Clamp(next,count);if(next!=target){from=Value(now);target=next;started=now;}}
    void Follow(int row,int count,unsigned now){if(row<target)Move(float(row),count,now);else if(row>=target+5)Move(float(row-4),count,now);}
    void Drag(float next,int count,unsigned now){from=target=Clamp(next,count);started=now;}
};
inline Rect MainItem(int row,float scroll){const int y=58+row*26-int(scroll*26+0.5f);return {44,y,276,y+24};}
inline int MainHit(int x,int y,int count,float scroll){
    if(x<44 || x>=276 || y<58 || y>=188)return -1;
    for(int row=0;row<count;++row)if(MainItem(row,scroll).Contains(x,y))return row;
    return -1;
}
// Original TV artwork leaves this native-pixel glass area clear of its bezel.
constexpr Rect TVGlass={43,35,277,208};
constexpr int TVRowHeight=26;
constexpr int TVPageSize=5;
inline int TVMenuStep(int count){return count<=3?34:count==4?30:TVRowHeight;}
inline int TVMenuTop(int count){return 123-count*TVMenuStep(count)/2;}
inline Rect TVMenuItem(int index,int count){const int y=TVMenuTop(count)+index*TVMenuStep(count);return {44,y,276,y+TVMenuStep(count)-2};}
inline int TVMenuHit(int x,int y,int count){for(int i=0;i<count;++i)if(TVMenuItem(i,count).Contains(x,y))return i;return -1;}
inline Rect TVThumbnail(int index){return {44+(index%4)*58,57+(index/4)*63,96+(index%4)*58,117+(index/4)*63};}
inline int TVThumbnailHit(int x,int y,int count){for(int i=0;i<count && i<8;++i)if(TVThumbnail(i).Contains(x,y))return i;return -1;}
// One layout supplies both rendering and hit testing at native 320 x 240.
inline Rect PauseItem(int index,int count)
{
    if(index==0)return {10,40,310,76};
    const int row=(index-1)/2,column=(index-1)%2;
    const int height=count>7 ? 27 : 38;
    return {10+column*153,82+row*(height+5),157+column*153,82+row*(height+5)+height};
}
inline Rect Thumbnail(int index){return {10+(index%4)*77,40+(index/4)*83,79+(index%4)*77,118+(index/4)*83};}
inline int ThumbnailHit(int x,int y,int count){for(int i=0;i<count && i<8;++i)if(Thumbnail(i).Contains(x,y))return i;return -1;}
inline int PauseHit(int x,int y,int count)
{
    for(int i=0;i<count;++i)if(PauseItem(i,count).Contains(x,y))return i;
    return -1;
}
inline int PauseMove(int index,int count,int dx,int dy)
{
    if(index<0 || count<2)return 0;
    if(index==0)return dy>0 ? 1 : dy<0 ? count-1 : 0;
    if(dx){int other=((index-1)%2) ? index-1 : index+1;return other<count?other:index;}
    if(dy<0)return index<=2 ? 0 : index-2;
    if(dy>0)return index+2<count ? index+2 : 0;
    return index;
}
inline Rect MenuItem(int index,int count)
{
    const int y=MenuTop(count)+index*MenuStep(count);
    return {12,y,308,y+MenuStep(count)-4};
}
inline int MenuHit(int x,int y,int count)
{
    for(int i=0;i<count;++i)if(MenuItem(i,count).Contains(x,y))return i;
    return -1;
}
// A label tap selects its row. Only a press on the track captures a slider.
inline bool SliderTrackHit(int x,bool television){return x>=(television?188:196) && x<(television?273:305);}
inline float SliderValue(int x,bool television){float value=(x-(television?194:202))/(television?72.0f:96.0f);return value<0?0:value>1?1:value;}
struct Point { float x,y; };
struct Vertex { Point position; unsigned colour; };
struct Mesh {
    static constexpr unsigned Count=16*6*4;
    Vertex vertices[Count];
    unsigned used=0;
    void Outline(Point (&points)[16],float left,float top,float right,float bottom,float radius)
    {
        const float offsets[4][2]={{1,0},{0.8660254f,0.5f},{0.5f,0.8660254f},{0,1}};
        for(unsigned corner=0;corner<4;++corner)for(unsigned i=0;i<4;++i){
            const float a=offsets[i][0]*radius,b=offsets[i][1]*radius;
            switch(corner){
                case 0: points[corner*4+i]={right-radius+a,bottom-radius+b};break;
                case 1: points[corner*4+i]={left+radius-b,bottom-radius+a};break;
                case 2: points[corner*4+i]={left+radius-a,top+radius-b};break;
                default:points[corner*4+i]={right-radius+b,top+radius-a};break;
            }
        }
    }
    void Ring(float l,float t,float r,float b,float radius,float il,float it,float ir,float ib,float innerRadius,unsigned light,unsigned dark)
    {
        Point outer[16],inner[16];Outline(outer,l,t,r,b,radius);Outline(inner,il,it,ir,ib,innerRadius);
        for(unsigned i=0;i<16;++i){
            const unsigned next=(i+1)%16,colour=i>=6 && i<=13?light:dark;
            vertices[used++]={outer[i],colour};vertices[used++]={outer[next],colour};vertices[used++]={inner[i],colour};
            vertices[used++]={inner[i],colour};vertices[used++]={outer[next],colour};vertices[used++]={inner[next],colour};
        }
    }
    Mesh()
    {
        Ring(0,0,Width,Height,0,5,4,315,236,24,0xff201b39,0xff201b39);
        Ring(5,4,315,236,24,11,9,309,232,22,0xff665784,0xff211a3d);
        Ring(11,9,309,232,22,17,14,303,225,20,0xff46365f,0xff35264f);
        Ring(17,14,303,225,20,Left,Top,Right,Bottom,18,0xff100e1b,0xff191321);
    }
};
inline const Mesh& Bezel(){static const Mesh mesh;return mesh;}
}
#endif
