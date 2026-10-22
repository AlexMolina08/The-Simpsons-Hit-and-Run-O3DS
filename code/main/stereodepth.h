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

    inline float ScreenDistance(Camera camera)
    {
        switch(camera)
        {
            case Walking: return 3.5f;
            case Cinematic: return 5.0f;
            case Close: return 2.5f;
            default: return 7.0f;
        }
    }

    struct State
    {
        float convergence;
        State() : convergence(7.0f) {}
        void Reset() { convergence = 7.0f; }
        void Update(Camera camera, unsigned elapsedMilliseconds)
        {
            // A slow frame must not cause a sudden depth jump. Profiles change
            // with camera mode, rather than continuously refocusing the scene.
            const unsigned elapsed = elapsedMilliseconds > 50 ? 50 : elapsedMilliseconds;
            const float alpha = float(elapsed) / (250.0f + float(elapsed));
            convergence += (ScreenDistance(camera) - convergence) * alpha;
        }
        float Shift(float slider) const { return MaximumShift * SliderStrength(slider); }
    };
}
#endif
