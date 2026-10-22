#ifndef VEHICLEPREVIEW_H
#define VEHICLEPREVIEW_H

#include <math.h>

namespace VehiclePreview
{
    const float Radius = 1.8f;
    const float Distance = 4.6f;
    const float ScreenDistance = 4.8f;
    const float Pitch = -0.16f;
    const float InitialYaw = 0.6f;

    // Fit the complete yaw envelope once when a model loads. A cylindrical
    // bound keeps every rotation clear of the title, footer and stereo edges.
    inline float FitRadius(float horizontal, float bottom, float top)
    {
        if(!(horizontal > 0 && horizontal < 2 && bottom >= -2 && top <= 2 && bottom <= top))
            return 1.0f;
        const float focal = 200.0f / tanf(0.5f);
        const float upper = (120.0f - 60.0f) / focal;
        const float lower = (120.0f - 201.0f) / focal;
        const float side = (200.0f - 25.0f - 3.0f) / focal;
        const float sine = sinf(Pitch), cosine = cosf(Pitch);
        float radius = Radius;
        for(int vertical = 0; vertical < 2; ++vertical)
        {
            const float y = vertical ? top : bottom;
            for(int end = 0; end < 2; ++end)
            {
                const float z = end ? horizontal : -horizontal;
                const float eyeY = cosine * y - sine * z;
                const float eyeZ = sine * y + cosine * z;
                const float above = eyeY - upper * eyeZ;
                const float below = lower * eyeZ - eyeY;
                const float edge = horizontal - side * eyeZ;
                if(above > 0) radius = fminf(radius, upper * Distance / above);
                if(below > 0) radius = fminf(radius, -lower * Distance / below);
                if(edge > 0) radius = fminf(radius, side * Distance / edge);
            }
        }
        return radius;
    }

    inline float Rotate(float yaw, unsigned elapsedMilliseconds, bool left, bool right)
    {
        const unsigned elapsed = elapsedMilliseconds > 100 ? 100 : elapsedMilliseconds;
        yaw += float(int(right) - int(left)) * float(elapsed) * 0.0016f;
        const float turn = 6.283185307f;
        if(yaw > turn) yaw -= turn;
        if(yaw < -turn) yaw += turn;
        return yaw;
    }
}
#endif
