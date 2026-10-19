#ifndef PICA_UNLIT_PROJECTION_HPP
#define PICA_UNLIT_PROJECTION_HPP
#include <cmath>
namespace picaUnlitProjection {
// Rows are the actual hardware projection, after viewport tilt/depth conversion.
// Other projections retain the complete original shader path.
inline bool Parts(const float p[4][4],float scale[4]) {
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c)
        if(!std::isfinite(p[r][c])) return false;
    if(p[0][0]!=0 || p[0][2]!=0 || p[0][3]!=0 ||
       p[1][1]!=0 || p[1][2]!=0 || p[1][3]!=0 ||
       p[2][0]!=0 || p[2][1]!=0 ||
       p[3][0]!=0 || p[3][1]!=0 || p[3][3]!=0) return false;
    scale[0]=p[0][1];scale[1]=p[1][0];scale[2]=p[2][2];scale[3]=p[3][2];
    return true;
}
}
#endif
