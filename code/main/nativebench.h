#ifndef SRR_NATIVEBENCH_H
#define SRR_NATIVEBENCH_H
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <3ds.h>
namespace NativeBench {
// Only the automation's existing whole-update/render/stream clocks are retained.
enum Stage { Update, Render, Stream, Count };
enum Counter { ScopeCalls, CountC };
extern bool detail, collecting,validation;
void Hash(unsigned);
void HashFloat(float);
extern unsigned counters[CountC];
struct Scope {
    explicit Scope(Stage s):start(0){if(collecting && (detail || s<=Render))BeginScope(s);}
    ~Scope(){if(start)EndScope();}
    void BeginScope(Stage);void EndScope();
    Stage stage;u64 start,child;Scope* parent;
};
void Init(int argc=0,char** argv=nullptr); unsigned Seed(unsigned); unsigned Begin(unsigned elapsed,int context);
bool ResidentShaders();
bool NativeUniforms();
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
