#ifndef PICA_PREPARED_BATCH_HPP
#define PICA_PREPARED_BATCH_HPP
#include <pddi/pica/prepareduniforms.hpp>
#include <cstdint>

struct picaPreparedUniformRecord {
    picaPreparedTransformUniforms transform;
    uint16_t view,light,material;
};
// Keys are producer metadata. Consumer reads only retained component values and
// draw indices. Never reset a batch while any consumer still references it.
class picaPreparedInputPool {
public:
    static constexpr unsigned Capacity=16;
    struct ViewKey {unsigned epoch;bool stereo,active;};
    struct LightKey {unsigned epoch,ambient;};
    struct MaterialKey {const void* identity;unsigned uid,ambient,diffuse,emissive;bool lit,prepared;};
    struct Selection {uint16_t view,light,material;bool newView,newLight,newMaterial;};
    picaPreparedViewUniforms views[Capacity];
    picaPreparedLightUniforms lights[Capacity];
    picaPreparedMaterialUniforms materials[Capacity];
    unsigned viewCount=0,lightCount=0,materialCount=0,drawCount=0;
    unsigned resets=0;
    void Reset(){viewCount=lightCount=materialCount=drawCount=0;++resets;}
    bool Reserve(ViewKey view,LightKey light,MaterialKey material,Selection& output,bool retainedMaterial=false)
    {
        unsigned v=0,l=0,m=0;
        for(;v<viewCount;++v)if(viewKeys[v].epoch==view.epoch && viewKeys[v].stereo==view.stereo && viewKeys[v].active==view.active)break;
        for(;l<lightCount;++l)if(lightKeys[l].epoch==light.epoch && lightKeys[l].ambient==light.ambient)break;
        for(;!retainedMaterial && m<materialCount;++m){const MaterialKey& key=materialKeys[m];
            if(key.identity==material.identity && key.uid==material.uid && key.ambient==material.ambient &&
               key.diffuse==material.diffuse && key.emissive==material.emissive && key.lit==material.lit && key.prepared==material.prepared)break;}
        if(drawCount==Capacity || v==Capacity || l==Capacity || m==Capacity)return false;
        output={uint16_t(v),uint16_t(l),uint16_t(m),v==viewCount,l==lightCount,!retainedMaterial && m==materialCount};
        // All capacity checks precede mutation: callers can fence/reset/retry.
        if(output.newView){viewKeys[viewCount++]=view;}
        if(output.newLight){lightKeys[lightCount++]=light;}
        if(output.newMaterial){materialKeys[materialCount++]=material;}
        ++drawCount;
        return true;
    }
    picaPreparedUniformView View(const picaPreparedUniformRecord& input,const picaPreparedMaterialUniforms* retained=nullptr) const
    {
        return picaPreparedUniformView(views[input.view],lights[input.light],retained?*retained:materials[input.material],input.transform);
    }
private:
    ViewKey viewKeys[Capacity];LightKey lightKeys[Capacity];MaterialKey materialKeys[Capacity];
};
static_assert(sizeof(picaPreparedUniformRecord)<=80,"Compact transform and component indices");
static_assert(sizeof(picaPreparedInputPool)<=16384,"Bounded retained input storage");
#endif
