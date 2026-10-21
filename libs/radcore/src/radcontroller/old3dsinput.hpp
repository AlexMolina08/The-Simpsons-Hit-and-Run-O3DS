#ifndef SRR2_OLD3DSINPUT_HPP
#define SRR2_OLD3DSINPUT_HPP

namespace Old3DSInput
{
    struct TouchActionPulse
    {
        static constexpr unsigned Duration=150;
        unsigned started=0;bool pending=false;
        void Request(unsigned now){started=now;pending=true;}
        void Cancel(){pending=false;}
        bool Value(unsigned now){if(pending && now-started<Duration)return true;pending=false;return false;}
    };
    struct SelectChord
    {
        bool wasDown = false;
        bool cameraChord = false;

        bool Value(bool down, bool cameraHeld, bool cameraPoint)
        {
            // Latch the modifier at the Select edge. Releasing L first must
            // never turn a camera-cycle chord into a vehicle reset.
            if(down && !wasDown)
                cameraChord = cameraHeld;
            wasDown = down;
            return down && (cameraChord == cameraPoint);
        }
    };

    // Use raw signed SDL axis values. Normalization and Y inversion remain in
    // the controller layer, shared with the original right-stick camera.
    inline int MovementAxis(bool cameraHeld, int movement)
    {
        return cameraHeld ? 0 : movement;
    }

    inline int CameraAxis(bool cameraHeld, int movement, int rightStick)
    {
        return cameraHeld ? movement : rightStick;
    }
}

#endif
