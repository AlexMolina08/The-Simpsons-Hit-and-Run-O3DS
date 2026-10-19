#ifndef PICA_BOTTOM_FRAME_HPP
#define PICA_BOTTOM_FRAME_HPP

namespace picaBottomFrame {
constexpr float Width=320, Height=240;
constexpr float Left=21, Top=18, Right=299, Bottom=221;
constexpr int MenuRowHeight=24;
inline int MenuTop(int count){return 120-count*MenuRowHeight/2;}
inline int MenuHit(int x,int y,int count)
{
    const int top=MenuTop(count);
    return x>=34 && x<286 && y>=top && y<top+count*MenuRowHeight
        ? (y-top)/MenuRowHeight : -1;
}
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
