#ifndef SIM_JOINT_PIPELINE_HPP
#define SIM_JOINT_PIPELINE_HPP

namespace sim { namespace jointPipeline {
// A view lives for one read-only articulated synchronization. It retains no
// matrices or object identities across drivers, substeps, pose changes or clones.
template<class Matrix> struct View
{
    static const unsigned Capacity = 64;
    const Matrix* matrices[Capacity] = {};
    unsigned hits = 0, misses = 0;
    template<class Fetch> const Matrix& Resolve(unsigned index, Fetch fetch)
    {
        if(index >= Capacity) return fetch();
        if(matrices[index]) { ++hits; return *matrices[index]; }
        const Matrix& result = fetch();
        matrices[index] = &result;
        ++misses;
        return result;
    }
};
template<class Matrix> class Scope
{
    View<Matrix>*& target;
    View<Matrix>* previous;
public:
    Scope(View<Matrix>*& active, View<Matrix>* current):target(active),previous(active)
        { target=current; }
    ~Scope() { target=previous; }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
} }
#endif
