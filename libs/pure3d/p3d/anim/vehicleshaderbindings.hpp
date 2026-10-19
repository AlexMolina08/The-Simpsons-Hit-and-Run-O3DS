#ifndef P3D_PREPARED_VEHICLE_SHADERS_HPP
#define P3D_PREPARED_VEHICLE_SHADERS_HPP

#include <new>
#include <typeinfo>

// Only exact geometry has the audited primitive-group callback contract.
// Other drawables (including traffic wrappers and effects) retain virtual dispatch.
class tPreparedVehicleShaders
{
    struct Entry
    {
        tDrawable* source = NULL;
        tGeometry* geometry = NULL;
        unsigned revision = 0, first = 0, count = 0;
    };
    Entry* entries = NULL;
    tPrimGroup** slots = NULL;
    unsigned size = 0;
public:
    unsigned preparedCalls = 0, legacyCalls = 0, slotCalls = 0;
    ~tPreparedVehicleShaders()
    {
        for(unsigned i=0; i<size; ++i)
            if(entries[i].source) entries[i].source->Release();
        delete[] slots;
        delete[] entries;
    }
    bool Prepare(tCompositeDrawable& composite)
    {
        const unsigned count = composite.GetNumDrawableElement();
        if(count > 64 || entries) return false;
        entries = new(std::nothrow) Entry[count];
        if(!entries && count) return false;
        size = count;
        unsigned total = 0;
        for(unsigned i=0; i<count; ++i)
        {
            auto* element = composite.GetDrawableElement(i);
            tDrawable* source = element ? element->GetDrawable() : NULL;
            entries[i].source = source;
            if(!source) continue;
            source->AddRef(); // Protect identity against replacement and allocator reuse.
            if(typeid(*source) != typeid(tGeometry)) continue;
            tGeometry* geometry = static_cast<tGeometry*>(source);
            const int groups = geometry->GetNumPrimGroup();
            if(groups < 0 || unsigned(groups) > 256-total) return false;
            entries[i].geometry = geometry;
            entries[i].revision = geometry->ShaderBindingRevision();
            entries[i].first = total;
            entries[i].count = groups;
            total += groups;
        }
        slots = new(std::nothrow) tPrimGroup*[total];
        if(!slots && total) return false;
        for(unsigned i=0; i<count; ++i)
            for(unsigned j=0; j<entries[i].count; ++j)
                slots[entries[i].first+j] = entries[i].geometry->GetPrimGroup(j);
        return true;
    }
    void Process(unsigned index, tDrawable* current, tDrawable::ShaderCallback& callback)
    {
        if(!current) return;
        if(index<size)
        {
            const Entry& entry = entries[index];
            if(current==entry.source && entry.geometry && entry.revision != ~0u &&
               entry.geometry->ShaderBindingRevision()==entry.revision)
            {
                ++preparedCalls;
                for(unsigned j=0; j<entry.count; ++j)
                {
                    tPrimGroup* group = slots[entry.first+j];
                    tShader* shader = group->GetShader();
                    tShader* replacement = callback.Process(shader);
                    const bool changed=entry.geometry->ShaderBindingRevision()!=entry.revision;
                    if(replacement != shader)
                        (changed ? entry.geometry->GetPrimGroup(j) : group)->SetShader(replacement);
                    ++slotCalls;
                    // A callback may replace primitive groups. The legacy loop reads
                    // the next live slot, so continue through the live geometry too.
                    if(changed)
                    {
                        for(int next=int(j)+1; next<entry.geometry->GetNumPrimGroup(); ++next)
                        {
                            tPrimGroup* live = entry.geometry->GetPrimGroup(next);
                            tShader* before = live->GetShader();
                            tShader* after = callback.Process(before);
                            if(after!=before) live->SetShader(after);
                            ++slotCalls;
                        }
                        return;
                    }
                }
                return;
            }
        }
        ++legacyCalls;
        current->ProcessShaders(callback);
    }
};
#endif
