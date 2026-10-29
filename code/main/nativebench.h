#ifndef SRR_NATIVEBENCH_H
#define SRR_NATIVEBENCH_H
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <3ds.h>
namespace NativeBench {
bool LandmarkDiagnostic();
// Only the automation's existing whole-update/render/stream clocks are retained.
enum Stage { Update, Render, Stream, Service, FileService, DebugService, SoundService, FileLock, FileCallback, FileError, FileUserCallback, SoundBufferComplete, SoundEncode, SoundEncodeKernel, LoadHandoff, LoadCallback, LoadYield, LoadLock, Count };
enum Counter { ScopeCalls, AdpcmClips, AdpcmSamples, CountC };
extern bool detail, collecting,validation,serviceProfile;
#ifdef SRR2_OLD3DS_SUBMIT_PROFILE
extern bool submissionProfile;
#endif
bool OnCaptureThread();
void Hash(unsigned);
void HashFloat(float);
extern unsigned counters[CountC];
struct Scope {
    explicit Scope(Stage s):start(0){if(s<=Service ? collecting : (OnCaptureThread() && collecting && (detail || serviceProfile)))BeginScope(s);}
    ~Scope(){if(start)EndScope();}
    void BeginScope(Stage);void EndScope();
    Stage stage;u64 start,child;Scope* parent;
};
void SaveFilename(char*,unsigned,unsigned);
void Init(int argc=0,char** argv=nullptr); unsigned Seed(unsigned); unsigned Begin(unsigned elapsed,int context);
bool ResidentShaders();
unsigned LoaderSliceUs();
bool IdleLoaderWait();
unsigned CurrentRouteMs();
bool NativeUniforms();
bool StereoVisualAudit();
bool StereoClip();
bool EarlyWorldQueue();
unsigned EarlyWorldQueuePhase();
float SmallObjectCullSquared();
unsigned AmbientTrafficLimit(unsigned accepted);
void CountAmbientTrafficDisplay();
float StereoSlider(float physical);
bool End(u64 work,int context); void Dump();
void Add(Counter,unsigned=1);
}
extern "C" int nativeBenchInput(int axis,int id,int original);
#define NB_JOIN_(a,b) a##b
#define NB_JOIN(a,b) NB_JOIN_(a,b)
#define NB_SCOPE(s) NativeBench::Scope NB_JOIN(nbScope,__LINE__)(NativeBench::s)
#define NB_COUNT(s,n) NativeBench::Add(NativeBench::s,n)
#else
#define NB_SCOPE(s) ((void)0)
#define NB_COUNT(s,n) ((void)0)
#endif
#endif
