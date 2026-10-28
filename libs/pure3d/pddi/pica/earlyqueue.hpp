#ifndef PICA_EARLY_QUEUE_HPP
#define PICA_EARLY_QUEUE_HPP
#include <atomic>

// Citro3D arms swaps and its completion timer inside FrameEnd. While an early
// queue runs, defer that callback until FrameEnd has published those fields.
// Draw commands retain their original order. The final callback runs once even
// if the queue drains before, during or after FrameEnd. No wait is introduced.
namespace picaEarlyQueue {
struct State {
    gxCmdQueue_s* queue=nullptr;
    void (*completion)(gxCmdQueue_s*)=nullptr;
    std::atomic<unsigned> stage{0}; // 0 idle, 1 recording, 2 armed, 3 completed.
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    unsigned starts=0;
    std::atomic<unsigned> deferred{0},completed{0};
#endif
};
inline State& Current(){static State state;return state;}
inline void Complete(gxCmdQueue_s* queue){
    State& state=Current();
    if(state.stage.load(std::memory_order_acquire)==1){
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        state.deferred.fetch_add(1,std::memory_order_relaxed);
#endif
        return;
    }
    // A callback queued by an earlier drain can arrive after more entries were
    // appended. Only the final, fully completed queue may publish the frame.
    if(__atomic_load_n(&queue->lastEntry,__ATOMIC_ACQUIRE)!=
       __atomic_load_n(&queue->numEntries,__ATOMIC_ACQUIRE))return;
    unsigned expected=2;
    if(state.stage.compare_exchange_strong(expected,3,std::memory_order_acq_rel)){
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        state.completed.fetch_add(1,std::memory_order_relaxed);
#endif
        state.completion(queue);
    }
}
inline void Start(gxCmdQueue_s* queue){
    State& state=Current();
    if(state.stage.load(std::memory_order_acquire))return;
    if(!queue->callback)return;
    state.queue=queue;state.completion=queue->callback;
    state.stage.store(1,std::memory_order_release);
    queue->callback=Complete;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    ++state.starts;
#endif
    gxCmdQueueRun(queue);
}
inline void Arm(){
    State& state=Current();
    unsigned expected=1;
    if(!state.stage.compare_exchange_strong(expected,2,std::memory_order_acq_rel))return;
    // FrameEnd has enqueued every transfer. No producer changes numEntries now.
    if(__atomic_load_n(&state.queue->lastEntry,__ATOMIC_ACQUIRE)==
       __atomic_load_n(&state.queue->numEntries,__ATOMIC_ACQUIRE))Complete(state.queue);
}
inline void Restore(){
    State& state=Current();
    if(!state.queue)return;
    // Caller already drained the SDK queue before reuse or destruction.
    Complete(state.queue);
    state.queue->callback=state.completion;
    state.queue=nullptr;state.stage.store(0,std::memory_order_release);
}
}
#endif
