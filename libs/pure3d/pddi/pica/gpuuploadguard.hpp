#ifndef PICA_GPUUPLOADGUARD_HPP
#define PICA_GPUUPLOADGUARD_HPP

// The loader mutex may be lent while Citro3D begins a frame. The public backend
// in-frame flag is still false then, although Citro3D can already queue copies
// asynchronously. Block loader-side synchronous uploads for this whole handoff.
namespace picaUpload {
extern unsigned gpuHandoff;
inline bool Blocked() { return __atomic_load_n(&gpuHandoff,__ATOMIC_ACQUIRE)!=0; }
class Handoff {
    unsigned previous;
public:
    Handoff():previous(__atomic_load_n(&gpuHandoff,__ATOMIC_RELAXED))
    { __atomic_store_n(&gpuHandoff,1u,__ATOMIC_RELEASE); }
    ~Handoff() { __atomic_store_n(&gpuHandoff,previous,__ATOMIC_RELEASE); }
    Handoff(const Handoff&)=delete;
    Handoff& operator=(const Handoff&)=delete;
};
}
#endif
