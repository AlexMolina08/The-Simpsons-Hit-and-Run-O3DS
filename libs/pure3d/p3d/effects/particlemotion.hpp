#ifndef P3D_PARTICLE_MOTION_HPP
#define P3D_PARTICLE_MOTION_HPP
#include <cmath>
namespace p3dParticleMotion {
// Only an ordinary perspective point camera can prove the legacy projected
// positions and normalized motion angle finite before doing those calculations.
struct View {
    float z[4]={};bool valid=false;
    bool Prepare(const float matrix[4][4],float fov,float aspect)
    {
        valid=false;
        if(!(fov>=0.01f && fov<=3.13f && aspect>=0.1f && aspect<=10.f))return false;
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
            if(!(std::fabs(matrix[row][col])<=(row==3?1000000.f:16.f)))return false;
        if(matrix[0][3]!=0 || matrix[1][3]!=0 || matrix[2][3]!=0 || matrix[3][3]!=1)return false;
        for(unsigned row=0;row<4;++row)z[row]=matrix[row][2];
        return valid=true;
    }
    bool FiniteAngle(const float* current,const float* previous) const
    {
        if(!valid)return false;
        for(unsigned axis=0;axis<3;++axis)
            if(!(std::fabs(current[axis])<=1000000.f && std::fabs(previous[axis])<=1000000.f))return false;
        const float currentZ=z[0]*current[0]+z[1]*current[1]+z[2]*current[2]+z[3];
        const float previousZ=z[0]*previous[0]+z[1]*previous[1]+z[2]*previous[2]+z[3];
        return std::fabs(currentZ)>=0.001f && std::fabs(previousZ)>=0.001f;
    }
};
}
#endif
