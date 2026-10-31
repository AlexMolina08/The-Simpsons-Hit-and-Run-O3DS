#ifndef OLD3DS_SUSPENSION_H
#define OLD3DS_SUSPENSION_H

namespace Old3dsSuspension {
// The parked car's spring/damper system can oscillate at ordinary 30 FPS when
// the performance policy permits a single long step. Refine only a nearby,
// upright car without throttle and with little horizontal motion. Vertical
// velocity must not exclude a car already bouncing on its suspension.
inline unsigned StepLimit(unsigned accepted, float throttle, float velocityX,
                          float velocityZ, float upY, float distanceSquared)
{
    if(accepted > 25 && throttle >= 0.0f && throttle < 0.1f &&
       velocityX * velocityX + velocityZ * velocityZ < 4.0f &&
       upY > 0.9f && upY <= 1.001f &&
       distanceSquared >= 0.0f && distanceSquared <= 2500.0f)
        return 25;
    return accepted;
}
}
#endif
