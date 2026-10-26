#ifndef RAD_NATIVE_SEEK_HPP
#define RAD_NATIVE_SEEK_HPP
#include <cstdio>

// Private benchmark experiment. Only a loader worker seeking its own native
// cache FILE may lend the mutex. Parsing, reads into GPU storage and publication
// remain under the existing loader mutex. The same owner is reclaimed on exit.
// Optional input staging also lends during private reads and hashing; GPU
// destination writes still wait until the same owner has been reclaimed.
namespace radNativeSeek {
extern bool enabled;
extern bool stageInput;
void* Begin();
void End(void* owner);
void Drain();
void Dump(FILE*);
class Lease {
    void* owner;
public:
    Lease():owner(enabled?Begin():nullptr){}
    ~Lease(){if(owner)End(owner);}
    Lease(const Lease&)=delete;
    Lease& operator=(const Lease&)=delete;
};
}
#endif
