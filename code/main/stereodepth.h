#ifndef STEREODEPTH_H
#define STEREODEPTH_H

// Stereo calibration uses world units for the screen plane and normalized
// screen coordinates for disparity. No SDK, allocation, or scene traversal.
namespace StereoDepth
{
    enum Camera { Default, Driving, Walking, Cinematic, Close };
    const float MaximumShift = 0.045f;
    const float SliderDeadzone = 0.01f;

    inline float SliderStrength(float slider)
    {
        if(!(slider > SliderDeadzone)) return 0.0f;
        if(slider >= 1.0f) return 1.0f;
        // Keep each part of the slider equally useful. A quadratic response
        // compressed the lower half into very small, hard-to-distinguish depth.
        return (slider - SliderDeadzone) / (1.0f - SliderDeadzone);
    }

    inline float ScreenDistance(Camera camera, float subjectDistance = 0.0f)
    {
        if((camera == Driving || camera == Walking) && subjectDistance > 0.0f && subjectDistance < 1000.0f)
        {
            // Follow the camera's subject, with a small amount of foreground
            // depth. Bound close/far camera changes before smoothing them.
            const float minimum = camera == Walking ? 1.0f : 2.0f;
            const float maximum = camera == Walking ? 6.0f : 14.0f;
            const float distance = subjectDistance * 1.08f;
            return distance < minimum ? minimum : distance > maximum ? maximum : distance;
        }
        switch(camera)
        {
            case Walking: return 3.5f;
            case Cinematic: return 5.0f;
            case Close: return 2.5f;
            default: return 7.0f;
        }
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
        State() : convergence(7.0f) {}
        void Reset() { convergence = 7.0f; }
        void Update(Camera camera, unsigned elapsedMilliseconds, float subjectDistance = 0.0f)
        {
            // A slow frame must not cause a sudden depth jump. Profiles change
            // with the subject distance, never with whatever crosses the view.
            const unsigned elapsed = elapsedMilliseconds > 50 ? 50 : elapsedMilliseconds;
            const float alpha = float(elapsed) / (250.0f + float(elapsed));
            convergence += (ScreenDistance(camera, subjectDistance) - convergence) * alpha;
        }
        float Shift(float slider) const { return MaximumShift * SliderStrength(slider); }
    };
}
#endif
