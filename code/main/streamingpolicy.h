#ifndef SRR_STREAMING_POLICY_H
#define SRR_STREAMING_POLICY_H
#include <cmath>

namespace StreamingPolicy {
// One speculative section, plus one slot reserved for demand loading. No heap
// admission number proves that an arbitrary section fits: this is experimental.
inline bool Admit(unsigned sections, unsigned capacity, unsigned normalFree, unsigned linearFree)
{
    return sections + 2 <= capacity && normalFree >= 8u*1024*1024 && linearFree >= 3u*1024*1024;
}
inline float LeadSeconds(float speed, float observedMilliseconds)
{
    float seconds = observedMilliseconds * .001f + .5f;
    if(seconds < 1.f) seconds = 1.f;
    if(seconds > 2.5f) seconds = 2.5f;
    if(speed > 1.f && seconds*speed > 80.f) seconds = 80.f/speed;
    return seconds;
}
// Sphere distance to a swept centerline. The caller confirms the actual trigger
// shape before nomination; bounding spheres alone must never start a load.
inline float SweepDistanceSquared(float dx, float dy, float dz, float vx, float vy, float vz)
{
    const float length = vx*vx + vy*vy + vz*vz;
    float t = length > 0.f ? (dx*vx + dy*vy + dz*vz)/length : 0.f;
    if(t < 0.f) t = 0.f;
    if(t > 1.f) t = 1.f;
    dx -= vx*t; dy -= vy*t; dz -= vz*t;
    return dx*dx + dy*dy + dz*dz;
}

// Prefer a confirmed forward crossing. A nearby crossing also warms one adjacent
// section while walking, entering a car or turning, before velocity is reliable.
// Bounding spheres only reject candidates; the real trigger confirms nomination.
template<class Intersects>
float NominationRank(float dx,float dy,float dz,float radius,
                     float vx,float vy,float vz,Intersects intersects)
{
    const float distance=dx*dx+dy*dy+dz*dz;
    const float forwardReach=radius+12.f;
    if(SweepDistanceSquared(dx,dy,dz,vx,vy,vz)<=forwardReach*forwardReach)
        for(unsigned sample=0;sample<=8;++sample)
            if(intersects(sample*.125f,12.f))return sample*1000000.f+distance;
    const float nearbyReach=radius+64.f;
    if(distance<=nearbyReach*nearbyReach && intersects(0.f,64.f))return 9000000.f+distance;
    return 1e30f;
}
}
#endif
