#include "libs/radcore/src/radcontroller/old3dsinput.hpp"
#include <cassert>
#include <cstdio>

int main()
{
    // Sweep both axes, including SDL's asymmetric signed endpoints. Holding L
    // must never move the character, and release must restore the current input.
    for(int axis = -32768; axis <= 32767; ++axis)
    {
        assert(Old3DSInput::MovementAxis(false, axis) == axis);
        assert(Old3DSInput::MovementAxis(true, axis) == 0);
        assert(Old3DSInput::CameraAxis(true, axis, 123) == axis);
        assert(Old3DSInput::CameraAxis(false, axis, 123) == 123);
    }
    // Regression: releasing the modifier with the pad still held must not leave
    // camera motion latched. Optional Circle Pad Pro input still works normally.
    const bool held[] = {false, true, true, false, false};
    const int pad[] = {32767, 32767, -32768, -32768, 0};
    const int movement[] = {32767, 0, 0, -32768, 0};
    const int camera[] = {0, 32767, -32768, 0, 0};
    for(unsigned i = 0; i < 5; ++i)
    {
        assert(Old3DSInput::MovementAxis(held[i], pad[i]) == movement[i]);
        assert(Old3DSInput::CameraAxis(held[i], pad[i], 0) == camera[i]);
    }
    Old3DSInput::SelectChord cycle, reset;
    assert(cycle.Value(true, true, true));
    assert(!reset.Value(true, true, false));
    assert(cycle.Value(true, false, true));
    assert(!reset.Value(true, false, false));
    assert(!cycle.Value(false, false, true));
    assert(!reset.Value(false, false, false));
    assert(!cycle.Value(true, false, true));
    assert(reset.Value(true, false, false));
    std::puts("Camera input: full signed-axis sweep and modifier transitions passed");
}
