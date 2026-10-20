//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#include <radmath/trig.hpp>
#include <radmath/util.hpp>
#include <math.h>

namespace RadicalMathLibrary {
#ifdef SRR2_OLD_3DS
// The SDK's combined kernel shares range reduction and polynomial powers.
// Keep uncommon FP controls and large/nonfinite inputs on the original path.
extern "C" void sincosf(float, float*, float*);
bool old3dsFusedTrig=true;
unsigned old3dsFusedTrigCalls=0,old3dsFusedTrigFallbacks=0;
void Old3dsSinCos(float angle,float* sine,float* cosine)
{
    unsigned control;
    __asm__ __volatile__("vmrs %0, fpscr":"=r"(control)::"memory");
    unsigned bits;
    __builtin_memcpy(&bits,&angle,sizeof(bits));
    if((control & 0x03f79f00u)==0x03000000u && (bits & 0x7fffffffu)<=0x46000000u)
    {
        float s,c;
        sincosf(angle,&s,&c);
        *sine=s;*cosine=c;
        __atomic_fetch_add(&old3dsFusedTrigCalls,1u,__ATOMIC_RELAXED);
        return;
    }
    *sine=sinf(angle);
    *cosine=cosf(angle);
    __atomic_fetch_add(&old3dsFusedTrigFallbacks,1u,__ATOMIC_RELAXED);
}
#endif
//
// If you are planning on using ATanCoords you might want 
// to use ATan2 instead they are similar but ATan2 is 
// more standard and ATanCoords is legacy code
// that can give non standard results.
// Liberty Oct,4 2000
//
float ATanCoords(float x, float y) 
{
    int quad = 0;

    if(x==0.0f)
        return 0.0f;

    if(x < 0)
    {
        quad = 1;
        x = -x;
    }

    if(y < 0) 
    {
        quad ^= 3;
        y = -y;
    }

    float a = y / x;
    switch (quad)
    {
        case 0:
            return (ATan(a));
        case 1:
            return (PI - ATan(a));
        case 2:
            return (PI + ATan(a));
        case 3:
            return (PI_2 - ATan(a));
    }
    // should never reach here
    return 0;
}

//-------------------------------------------------------------------
void CartesianToPolar(float x, float y, float* magnitude, float* angle)
{
    // first find out the length of the hypotenuse (or the magnitude)
    *magnitude = Sqrt( (x * x) + (y * y) );
    // now find out the angle using arctan
    *angle = ATanCoords(x,y);
}

void PolarToCartesian(float magnitude, float angle, float* x, float* y)
{
    int quad = ( ( RadianToAngle(angle) & 0x1ff ) >> 2 );
    *x = Cos(angle) * magnitude;
    *y = Sin(angle) * magnitude;

    if( quad & 0x2 )
    {
        // if we in bottom part of graph, turn y upside down
        *y = -(*y);
    }
    if( ( quad == 1 ) || ( quad == 2 ) )
    {
        // if we're in the left part of the graph, turn x upside down
        *x = -(*x);
    }
}

void CartesianToSpherical(float x, float y, float z, float* magnitude, float* angletheta, float* anglephi)
{
    *magnitude = Sqrt( (x*x) + (y*y) + (z*z) );
    *anglephi = ACos(z/(*magnitude));
    *angletheta = 0.0f;
    float s = Sqrt((x*x)+(y*y));
    if (s>0.0f)
    {
        float yOverS = Clamp( y/s, -1.0f, 1.0f );
        *angletheta = ASin(yOverS);
        if  (x<0.0f)
            *angletheta = PI - *angletheta;
    }
}


// fRadius is magnitude of spherical vector
// fTheta is angle in XY plane from the positive X axis to projection of vector on XY plane
// fPhi is angle of vector from positive Z Axis
void SphericalToCartesian(float fRadius, float fTheta, float fPhi, float* x, float* y, float* z)
{
    float sinValue;
    float cosValue;
    SinCos( fPhi, &sinValue, &cosValue );

    *z = fRadius * cosValue;
    float fTemp = fRadius * sinValue;

    SinCos( fTheta, &sinValue, &cosValue );

    *x = fTemp * cosValue;
    *y = fTemp * sinValue;
}

}

