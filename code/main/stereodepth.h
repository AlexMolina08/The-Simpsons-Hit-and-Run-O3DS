#ifndef STEREODEPTH_H
#define STEREODEPTH_H

// Stereo calibration uses world units for the screen plane and normalized
// screen coordinates for disparity. No SDK, allocation, or scene traversal.
namespace StereoDepth
{
    enum Camera { Default, Driving, Walking, Cinematic, Close, Interior, Paused };
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
        // The conservative fallback bounds the whole frustum, not vertices. On a
        // 400-pixel display this permits 8 pixels behind and 6 pixels in front
        // at full slider, without breaking contact or triangle interpolation.
        return ValidNearPlane(nearPlane) * (1.0f + MaximumForegroundShift / MaximumShift);
    }

    inline float ScreenDistance(Camera camera, float subjectDistance = 0.0f, float nearPlane = 1.0f)
    {
        nearPlane = ValidNearPlane(nearPlane);
        const bool playerView = camera == Driving || camera == Walking || camera == Interior;
        const float maximumSubject = camera == Driving ? 30.0f : camera == Interior ? 12.0f : 16.0f;
        if(playerView && subjectDistance >= nearPlane && subjectDistance <= maximumSubject)
        {
            // Place the player's reference point just inside the window.
            // Clip distance is not the depth of the nearest visible surface.
            const float distance = subjectDistance * (camera == Interior ? 0.90f : 0.95f);
            return distance < nearPlane ? nearPlane : distance;
        }
        // Bumper views and authored cinematic cameras can put geometry on the
        // clipping plane. Keep their complete frustum inside the old budget.
        return FurthestScreenPlane(nearPlane);
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
        float anchor;
        Camera lastCamera;
        unsigned lastKey, clearMilliseconds;
        bool initialized, wasTransitioning;
        State() { Reset(); }
        void Reset()
        {
            convergence = anchor = FurthestScreenPlane(1.0f);
            lastCamera = Default; lastKey = clearMilliseconds = 0;
            initialized = wasTransitioning = false;
        }
        void Update(Camera camera, unsigned elapsedMilliseconds, float subjectDistance = 0.0f,
                    float nearPlane = 1.0f, unsigned cameraKey = 0,
                    bool transitioning = false, bool obstructed = false)
        {
            // A slow frame must not cause a sudden depth jump. Profiles change
            // with deliberate camera transitions, never with a depth sample or
            // an object crossing the view. Normal walking and jumps hold focus.
            const unsigned elapsed = elapsedMilliseconds > 50 ? 50 : elapsedMilliseconds;
            nearPlane = ValidNearPlane(nearPlane);
            const bool playerView = camera == Driving || camera == Walking || camera == Interior;
            const float maximumSubject = camera == Driving ? 30.0f : camera == Interior ? 12.0f : 16.0f;
            const bool validSubject = subjectDistance >= nearPlane && subjectDistance <= maximumSubject;
            const bool changed = !initialized || camera != lastCamera || cameraKey != lastKey;
            if(changed || transitioning || wasTransitioning || !playerView)
                anchor = ScreenDistance(camera, subjectDistance, nearPlane);
            initialized = !playerView || validSubject; lastCamera = camera; lastKey = cameraKey;
            wasTransitioning = transitioning;

            float target = anchor;
            const bool compressed = playerView && subjectDistance >= nearPlane &&
                subjectDistance < anchor * 0.75f;
            if(obstructed || compressed)
            {
                // Reuse the camera's existing collision result. No new rays,
                // geometry scans or GPU depth reads run for stereo.
                const float safe = FurthestScreenPlane(nearPlane);
                if(target > safe) target = safe;
                clearMilliseconds = 0;
                if(convergence > target) convergence = target;
            }
            else
            {
                // Intermittent camera contacts must not pump depth in and out.
                clearMilliseconds = clearMilliseconds < 250 ? clearMilliseconds + elapsed : 250;
                if(clearMilliseconds < 250 && !changed && convergence < target) target = convergence;
            }
            if((changed || !playerView) && convergence > target) convergence = target;
            const float alpha = float(elapsed) / (350.0f + float(elapsed));
            convergence += (target - convergence) * alpha;
            if(convergence < nearPlane) convergence = nearPlane;
        }
        float Shift(float slider) const { return MaximumShift * SliderStrength(slider); }
    };
}
#endif
