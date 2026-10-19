#ifndef PICA_FRAME_PIPELINE_HPP
#define PICA_FRAME_PIPELINE_HPP
#include <pddi/pica/preparedexecutor.hpp>

// Capture one ordered frame in bounded pages, then transfer exclusive SDK
// ownership to core 1. Main may run simulation while that frame completes.
// Join before any next render, resource mutation, foreign SDK call or shutdown.
// There is never more than one outstanding frame and no output command copy.
class picaFramePipeline {
public:
    struct Metrics {
        unsigned frames=0,synchronous=0,presented=0,draws=0,pages=0,maximumPages=0,failures=0;
        u64 executionTicks=0,joinTicks=0,prefixWaitTicks=0;
        unsigned prefixFrames=0;
        float processingMs=0,drawingMs=0;
        unsigned comparisons=0,mismatches=0,comparedDraws=0;
    };
    static picaFramePipeline* Create(picaPreparedExecutor*,picaPreparedInputPool*);
    ~picaFramePipeline();
    void Begin(picaPreparedExecutor*&,picaPreparedInputPool*&);
    // False leaves the current page untouched. Caller can drain synchronously
    // and begin a new capture rather than omit work or exceed the memory cap.
    bool Advance(picaPreparedExecutor*&,picaPreparedInputPool*&);
    struct Drain {const void* site=nullptr;unsigned calls=0,draws=0;u64 ticks=0;};
    picaPreparedExecutor* Flush(picaPreparedExecutor*&,picaPreparedInputPool*&,const void* site=nullptr);
    struct JoinSite {const void* site=nullptr;unsigned calls=0;u64 cpuTicks=0,gpuTicks=0;};
    void NoteJoin(const void*,u64,u64);
    unsigned JoinSiteCount()const{return joinSiteCount;}
    const JoinSite& JoinSiteAt(unsigned index)const{return joinSites[index];}
    unsigned DrainCount()const{return drainCount;}
    const Drain& DrainAt(unsigned index)const{return drains[index];}
    void Submit(bool present,bool compare=false,bool gpuPrefix=false);
    picaPreparedExecutor* Join(picaPreparedExecutor*&,picaPreparedInputPool*&);
    bool Recording()const{return recording;}
    bool InFlight()const{return inFlight;}
    const Metrics& CompletedMetrics()const{return completedMetrics;}
    unsigned Core()const{return core;}
    unsigned MainFp()const{return mainFp;}
    unsigned WorkerFp()const{return workerFp;}
    unsigned InitialAllowance()const{return initialAllowance;}
    unsigned ActualAllowance()const{return actualAllowance;}
private:
    static constexpr unsigned MaximumPages=48;
    struct Page {
        picaPreparedExecutor* executor=nullptr;
        picaPreparedInputPool* pool=nullptr;
        unsigned operations=0,geometry=0,palettes=0;
    } pages[MaximumPages];
    unsigned page=0,request=0,complete=0,ready=0,quit=0;
    unsigned core=~0u,mainFp=0,workerFp=0,initialAllowance=0,actualAllowance=0;
    Thread thread=nullptr;LightEvent wake,finished;
    bool recording=false,inFlight=false,present=false,allowanceSet=false;
    picaPreparedExecutor* result=nullptr;
    const char* failure=nullptr;
    bool compare=false,splitPrefix=false;
    u32* comparisonBase=nullptr;
    picaPreparedExecutor* originalPrimary=nullptr;
    void* beforeCheckpoint=nullptr;void* expectedCheckpoint=nullptr;void* actualCheckpoint=nullptr;
    u32* expectedCommands=nullptr;unsigned expectedCapacity=0,expectedWords=0,startOffset=0;
    picaPreparedUniformCache expectedUniforms;
    picaStateCache expectedState;
    Metrics metrics,completedMetrics;
    Drain drains[32];unsigned drainCount=0;
    JoinSite joinSites[32];unsigned joinSiteCount=0;
    static void Worker(void*);
    void Seal();
    void Execute();
    void RestoreProducer(picaPreparedExecutor*&,picaPreparedInputPool*&);
    bool Allocate(unsigned);
};
#endif
