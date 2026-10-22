#ifndef STEREODEPTH_H
#define STEREODEPTH_H

// Stereo calibration uses world units for the screen plane and normalized
// screen coordinates for disparity. No SDK, allocation, or scene traversal.
namespace StereoDepth
{
    enum Camera { Default, Driving, Walking, Cinematic, Close };
    const float MaximumShift = 0.02f;
    const float MaximumForegroundShift = 0.015f;
    const float SliderDeadzone = 0.01f;

    inline float SliderStrength(float slider)
    {
        if(!(slider > SliderDeadzone)) return 0.0f;
        if(slider >= 1.0f) return 1.0f;
        // Keep each part of the slider equally useful. A quadratic response
        // compressed the lower half into very small, hard-to-distinguish depth.
        return (slider - SliderDeadzone) / (1.0f - SliderDeadzone);
    }

    inline float ValidNearPlane(float nearPlane)
    {
        return nearPlane >= 0.01f && nearPlane <= 1000.0f ? nearPlane : 1.0f;
    }

    inline float FurthestScreenPlane(float nearPlane)
    {
        // Bound the entire projective frustum, not individual vertices. On a
        // 400-pixel display this permits 8 pixels behind and 6 pixels in front
        // at full slider, without breaking contact or triangle interpolation.
        return ValidNearPlane(nearPlane) * (1.0f + MaximumForegroundShift / MaximumShift);
    }

    inline float ScreenDistance(Camera camera, float subjectDistance = 0.0f, float nearPlane = 1.0f)
    {
        nearPlane = ValidNearPlane(nearPlane);
        float distance;
        switch(camera)
        {
            case Walking: distance = 3.5f; break;
            case Cinematic: distance = 5.0f; break;
            case Close: distance = 2.5f; break;
            default: distance = 7.0f; break;
        }
        if((camera == Driving || camera == Walking) && subjectDistance > 0.0f && subjectDistance < 1000.0f)
            // Keep the player inside the window; nearby world geometry can
            // cross it naturally. Never push the whole player forward.
            distance = subjectDistance * 0.75f;
        const float maximum = FurthestScreenPlane(nearPlane);
        return distance < nearPlane ? nearPlane : distance > maximum ? maximum : distance;
    }

    inline float ProjectionScale(float screenDistance, float objectScreenDistance)
    {
        // Scaling every clip component preserves size, z/w and clipping. It
        // changes only the stereo screen plane, without another shader or pass.
        if(!(objectScreenDistance >= 0.05f && objectScreenDistance <= 1000.0f)) return 1.0f;
        return screenDistance / objectScreenDistance;
    }

    struct State
    {
        float convergence;
        State() : convergence(FurthestScreenPlane(1.0f)) {}
        void Reset() { convergence = FurthestScreenPlane(1.0f); }
        void Update(Camera camera, unsigned elapsedMilliseconds, float subjectDistance = 0.0f, float nearPlane = 1.0f)
        {
            // A slow frame must not cause a sudden depth jump. Profiles change
            // with the subject distance, never with whatever crosses the view.
            const unsigned elapsed = elapsedMilliseconds > 50 ? 50 : elapsedMilliseconds;
            const float alpha = float(elapsed) / (250.0f + float(elapsed));
            convergence += (ScreenDistance(camera, subjectDistance, nearPlane) - convergence) * alpha;
            // A camera cut or an interior can change the near plane at once.
            // Enforce that frustum's budget immediately, while ordinary focus
            // changes retain the smooth transition above.
            nearPlane = ValidNearPlane(nearPlane);
            const float maximum = FurthestScreenPlane(nearPlane);
            if(convergence > maximum) convergence = maximum;
            if(convergence < nearPlane) convergence = nearPlane;
        }
        float Shift(float slider) const { return MaximumShift * SliderStrength(slider); }
    };
}
#endif
