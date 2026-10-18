#ifndef PICA_PREPARED_WORKER_HPP
#define PICA_PREPARED_WORKER_HPP
#include <pddi/pica/preparedexecutor.hpp>

// Two bounded input arenas and private output buffers. During an active lease,
// only Drain owns SDK globals. Main commits finished bytes through saved live
// buffer fields, without reading those globals until Finish joins the consumer.
class picaPreparedWorker {
public:
    static picaPreparedWorker* Create(picaPreparedExecutor*,picaPreparedInputPool*);
    ~picaPreparedWorker();
    bool Active()const{return active;}
    void Begin(bool compare=false);
    void Publish(picaPreparedExecutor*&,picaPreparedInputPool*&);
    picaPreparedExecutor* Finish(picaPreparedExecutor*&,picaPreparedInputPool*&);
    bool GeometryRoom(unsigned pendingGeometry)const;
    void Buffer(u32** data,u32* size,u32* offset)const;
    unsigned workerDraws=0,mainDraws=0,jobs=0,comparisons=0,mismatches=0;
    unsigned initialAllowance=0,actualAllowance=0,workerCore=~0u,mainFp=0,workerFp=0;
    u64 waitTicks=0,commitTicks=0;
private:
    static constexpr unsigned OutputWords=(picaPreparedExecutor::Capacity+1)*picaPreparedExecutor::DrawWordBound;
    struct Slot {picaPreparedExecutor* executor=nullptr;picaPreparedInputPool* pool=nullptr;u32* output=nullptr;unsigned words=0,bound=0;};
    Slot slots[2];
    Thread thread=nullptr;LightEvent wake,finished;
    unsigned published=0,completed=0,committed=0,owner=0,quit=0,ready=0,failure=0;
    unsigned producer=0,reserved=0;
    picaPreparedExecutor* last=nullptr;
    u32* liveBuffer=nullptr;u32 liveSize=0,liveOffset=0;
    bool active=false,allowanceSet=false,compare=false;
    picaPreparedExecutor* reference=nullptr;
    u32* referenceOutput=nullptr;
    void* beforeCheckpoint=nullptr;void* expectedCheckpoint=nullptr;void* actualCheckpoint=nullptr;
    static void Worker(void*);
    void Drain(bool);
    void Commit();
    void Wait(unsigned);
    void Seal();
    void CheckFailure();
};
#endif
