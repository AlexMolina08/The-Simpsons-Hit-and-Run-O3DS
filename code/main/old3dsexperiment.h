#ifndef OLD3DS_EXPERIMENT_H
#define OLD3DS_EXPERIMENT_H
#ifdef SRR2_OLD3DS_CAUSAL_DIAG
#include <3ds.h>
// Diagnostics only. Modes never alter simulation substep selection.
enum O3ExperimentMode { O3_REFERENCE, O3_LOW, O3_COVERAGE, O3_MIP, O3_SUPPRESS };
enum O3ExperimentScope { O3E_BACKEND, O3E_MATERIAL, O3E_SUBMIT, O3E_UNIFORM,
    O3E_STREAM_FLUSH, O3E_TEXTURE, O3E_CONVERT, O3E_PHYSICS, O3E_SUBSTEP,
    O3E_WORLD, O3E_RENDER, O3E_WAIT, O3E_SCOPE_COUNT };
extern unsigned g_o3Mode, g_o3WorldDepth;
extern bool g_o3Detail;
void O3ExperimentExternal(unsigned mode,bool capture);
void O3ExperimentExternalCapture(bool capture);
void O3ExperimentBegin(unsigned frame, bool gameplay);
void O3ExperimentEnd(u64 workTicks, unsigned elapsed);
void O3ExperimentPhase(unsigned phase,u64 ticks);
void O3ExperimentArm(bool fixed);
bool O3ExperimentFixed();
void O3ExperimentDump();
void O3ExperimentSubmit();
void O3ExperimentDraw(unsigned vertices);
void O3ExperimentMip(bool available);
class O3ExperimentTimer {
    unsigned slot; u64 begin, childBegin;
public:
    explicit O3ExperimentTimer(unsigned slot);
    ~O3ExperimentTimer();
};
class O3ExperimentWorld {
    O3ExperimentTimer timer;
public:
    O3ExperimentWorld():timer(O3E_WORLD) { ++g_o3WorldDepth; }
    ~O3ExperimentWorld() { --g_o3WorldDepth; }
};
#endif
#endif
