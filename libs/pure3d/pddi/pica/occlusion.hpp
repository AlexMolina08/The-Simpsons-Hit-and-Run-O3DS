#ifndef PICA_OCCLUSION_HPP
#define PICA_OCCLUSION_HPP
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

// Same-frame depth evidence only. A cell is usable only when an entire cell
// lies strictly inside a submitted triangle, including coordinate uncertainty.
// No previous-frame depth, bounding-box occluders or probabilistic visibility.
namespace picaOcclusion {
constexpr unsigned Width=48,Height=80,Cells=Width*Height,CellBudget=8192;
struct Triangle {float vertex[3][3];};
struct Geometry {float low[3],high[3];unsigned count;Triangle triangles[8];};
struct Point {float x,y,ex,ey,nearDepth,farDepth;bool valid;};
struct Transform {
    float coefficient[4][4],magnitude[4][4];bool valid;
    void Prepare(const float mv[4][4],const float projection[4][4]) {
        valid=false;
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
            if(!std::isfinite(mv[r][c]) || !std::isfinite(projection[r][c]) ||
               std::fabs(mv[r][c])>(c==3?1000000.f:16.f) || std::fabs(projection[r][c])>10000.f)return;
        if(mv[3][0]!=0 || mv[3][1]!=0 || mv[3][2]!=0 || mv[3][3]!=1)return;
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
            coefficient[r][c]=magnitude[r][c]=0;
            for(unsigned k=0;k<4;++k) {
                coefficient[r][c]+=projection[r][k]*mv[k][c];
                magnitude[r][c]+=std::fabs(projection[r][k])*std::fabs(mv[k][c]);
            }
        }
        valid=true;
    }
    Point Project(const float center[3],const float extent[3],float relativeError) const {
        Point result={};if(!valid)return result;
        float clip[4],error[4];
        for(unsigned r=0;r<4;++r) {
            clip[r]=coefficient[r][3];error[r]=magnitude[r][3]*relativeError+0.00001f;
            for(unsigned c=0;c<3;++c) {
                if(!std::isfinite(center[c]) || !std::isfinite(extent[c]) || extent[c]<0 ||
                   std::fabs(center[c])+extent[c]>1000000.f)return result;
                clip[r]+=coefficient[r][c]*center[c];
                error[r]+=std::fabs(coefficient[r][c])*extent[c]+
                    magnitude[r][c]*(std::fabs(center[c])+extent[c])*relativeError;
            }
        }
        const float w=clip[3],ew=error[3];
        if(!(w-ew>0.01f) || !std::isfinite(w+ew))return result;
        const float inverse=1.f/w,safeInverse=1.f/(w-ew);
        const float x=clip[0]*inverse,y=clip[1]*inverse,z=-clip[2]*inverse;
        const float ex=(error[0]+std::fabs(x)*ew)*safeInverse;
        const float ey=(error[1]+std::fabs(y)*ew)*safeInverse;
        const float ez=(error[2]+std::fabs(z)*ew)*safeInverse+0.0001f;
        result.x=(x+1)*(.5f*Width);result.y=(y+1)*(.5f*Height);
        // One extra physical pixel covers viewport/fixed-point rasterization.
        result.ex=ex*(.5f*Width)+.25f;result.ey=ey*(.5f*Height)+.25f;
        result.nearDepth=z-ez;result.farDepth=z+ez;
        result.valid=std::isfinite(result.x+result.y+result.ex+result.ey+z+ez);
        return result;
    }
};
struct Interval {float low,high;};
inline Interval Mul(Interval a,Interval b) {
    const float v[]={a.low*b.low,a.low*b.high,a.high*b.low,a.high*b.high};
    return {*std::min_element(v,v+4),*std::max_element(v,v+4)};
}
inline Interval Sub(Interval a,Interval b){return {a.low-b.high,a.high-b.low};}
struct Edge {float a,b,c;};
inline Edge ConservativeEdge(const Point& a,const Point& b) {
    const Interval ax={a.x-a.ex,a.x+a.ex},ay={a.y-a.ey,a.y+a.ey};
    const Interval bx={b.x-b.ex,b.x+b.ex},by={b.y-b.ey,b.y+b.ey};
    const Interval c=Sub(Mul(ax,by),Mul(ay,bx));
    Edge e={ay.low-by.high,bx.low-ax.high,c.low};
    // Coefficient bounds use nonnegative screen coordinates. Include accumulated
    // CPU float rounding and the minimum over all four corners of a whole cell.
    const float rounding=(std::fabs(e.a)*Width+std::fabs(e.b)*Height+std::fabs(e.c)+1)*.00001f;
    e.c+=std::min(0.f,e.a)+std::min(0.f,e.b)-rounding;
    return e;
}
struct Grid {
    float depth[Cells];uint16_t stamp[Cells]={};uint16_t generation=1;
    unsigned visits=0,triangles=0,covered=0;
    void Clear(bool newFrame=false) {
        if(!++generation){std::memset(stamp,0,sizeof(stamp));generation=1;}
        if(newFrame)visits=triangles=covered=0;
    }
    bool Hidden(const Point& box) const {
        if(!box.valid || box.nearDepth<=0 || box.farDepth>1.01f)return false;
        const float left=box.x-box.ex,right=box.x+box.ex,top=box.y-box.ey,bottom=box.y+box.ey;
        if(right<0 || left>=Width || bottom<0 || top>=Height)return false;
        const int x0=int(std::floor(std::max(0.f,left))),y0=int(std::floor(std::max(0.f,top)));
        const int x1=std::min(int(Width)-1,int(std::floor(std::min(float(Width)-.001f,right))));
        const int y1=std::min(int(Height)-1,int(std::floor(std::min(float(Height)-.001f,bottom))));
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x) {
            const unsigned i=y*Width+x;
            if(stamp[i]!=generation || !(depth[i]+.0001f<box.nearDepth))return false;
        }
        return true;
    }
    void Add(Point a,Point b,Point c,unsigned cull) {
        if(!a.valid || !b.valid || !c.valid || triangles>=256 || visits>=CellBudget)return;
        // First hardware audit is deliberately restricted to unclipped triangles.
        // This avoids treating unverified clip-intersection rounding as proof.
        for(const Point* p:{&a,&b,&c})
            if(p->x-p->ex<0 || p->x+p->ex>Width || p->y-p->ey<0 || p->y+p->ey>Height ||
               p->nearDepth<=0 || p->farDepth>=1)return;
        const float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        const Interval abx={b.x-b.ex-a.x-a.ex,b.x+b.ex-a.x+a.ex};
        const Interval aby={b.y-b.ey-a.y-a.ey,b.y+b.ey-a.y+a.ey};
        const Interval acx={c.x-c.ex-a.x-a.ex,c.x+c.ex-a.x+a.ex};
        const Interval acy={c.y-c.ey-a.y-a.ey,c.y+c.ey-a.y+a.ey};
        const Interval signedArea=Sub(Mul(abx,acy),Mul(aby,acx));
        if(signedArea.low<=0 && signedArea.high>=0)return;
        // PICA mode 1 keeps clockwise, mode 2 keeps counter-clockwise triangles.
        if((cull==1 && area>0) || (cull==2 && area<0) || cull>2)return;
        if(area<0)std::swap(b,c);
        const Edge edges[]={ConservativeEdge(a,b),ConservativeEdge(b,c),ConservativeEdge(c,a)};
        const int x0=std::max(0,int(std::floor(std::min({a.x-a.ex,b.x-b.ex,c.x-c.ex}))));
        const int y0=std::max(0,int(std::floor(std::min({a.y-a.ey,b.y-b.ey,c.y-c.ey}))));
        const int x1=std::min(int(Width)-1,int(std::ceil(std::max({a.x+a.ex,b.x+b.ex,c.x+c.ex}))));
        const int y1=std::min(int(Height)-1,int(std::ceil(std::max({a.y+a.ey,b.y+b.ey,c.y+c.ey}))));
        const float farDepth=std::max({a.farDepth,b.farDepth,c.farDepth})+.0001f;
        ++triangles;
        for(int y=y0;y<=y1 && visits<CellBudget;++y) {
            float e[3];for(unsigned k=0;k<3;++k)e[k]=edges[k].a*x0+edges[k].b*y+edges[k].c;
            for(int x=x0;x<=x1 && visits<CellBudget;++x) {
                ++visits;
                if(e[0]>0 && e[1]>0 && e[2]>0) {
                    const unsigned i=y*Width+x;
                    if(stamp[i]!=generation || farDepth<depth[i]){depth[i]=farDepth;stamp[i]=generation;++covered;}
                }
                for(unsigned k=0;k<3;++k)e[k]+=edges[k].a;
            }
        }
    }
};
}
#endif
