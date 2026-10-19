#ifndef PICA_DEFERRED_SKIN_HPP
#define PICA_DEFERRED_SKIN_HPP
#include <pddi/pdditype.hpp>
#include <pddi/pica/preparedskin.hpp>
#include <pddi/pica/preparedbackend.h>
#include <cmath>
#include <cstring>
#include <new>

extern bool g_picaDeferredSkinEnabled;

// Main owns references, capture and retirement. Core 1 reads the immutable rest,
// pose and view inputs and exclusively owns the lazy packed-palette cache.
namespace picaDeferredSkin {
constexpr unsigned MaximumJoints=256,MaximumSnapshots=64;
inline bool Bounded(const pddiMatrix& matrix)
{
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column){
        const float value=matrix.Row4(row)[column];
        if(!std::isfinite(value) || std::fabs(value)>1000000.0f)return false;
    }
    return true;
}
struct Rest {
    unsigned references=1,count=0;
    pddiMatrix* inverse=nullptr;
    static Rest* Create(const pddiMatrix* const* source,unsigned count)
    {
        if(!source || !count || count>=MaximumJoints)return nullptr;
        for(unsigned i=0;i<count;++i)if(!source[i] || !Bounded(*source[i]))return nullptr;
        auto* result=new(std::nothrow)Rest;if(!result)return nullptr;
        result->inverse=new(std::nothrow)pddiMatrix[count];
        if(!result->inverse){delete result;return nullptr;}
        result->count=count;
        for(unsigned i=0;i<count;++i)result->inverse[i]=*source[i];
        return result;
    }
    void Retain(){++references;}
    void Release(){if(!--references){delete[] inverse;delete this;}}
};
struct Pool;
struct Snapshot {
    Pool* pool=nullptr;Snapshot* nextFree=nullptr;Snapshot* nextAllocated=nullptr;
    Rest* rest=nullptr;unsigned references=0,computedCount=0;
    pddiMatrix pose[MaximumJoints],view;
    float packed[MaximumJoints][3][4];
    bool used[MaximumJoints],computed[MaximumJoints];
    void Retain(){++references;}
    void Release();
    void ResetConsumer(){std::memset(computed,0,sizeof(computed));computedCount=0;}
    bool Pack(unsigned index)
    {
        if(!rest || index>rest->count || !used[index])return false;
        if(computed[index])return true;
        pddiMatrix bone;
        if(index==rest->count)bone.Identity();
        else bone.Mult(rest->inverse[index],pose[index]);
        float source[4][4],eye[4][4];
        for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column){
            source[row][column]=bone.Row4(row)[column];eye[row][column]=view.Row4(row)[column];
        }
        if(!picaSkin::PackEyeMatrixValidatedView(packed[index],source,eye))return false;
        computed[index]=true;++computedCount;return true;
    }
};
using Stats=picaDeferredSkinStats;
struct Pool {
    Snapshot* available=nullptr;Snapshot* allocated=nullptr;Stats stats;
    ~Pool(){while(allocated){auto* next=allocated->nextAllocated;delete allocated;allocated=next;}}
    Snapshot* Capture(Rest* rest,const pddiMatrix* const* pose,const bool* used,const pddiMatrix& view)
    {
        float eye[4][4];
        for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)eye[row][column]=view.Row4(row)[column];
        if(!rest || !pose || !used || !Bounded(view) || !picaSkin::EyeViewValid(eye)){++stats.inputFallbacks;return nullptr;}
        for(unsigned i=0;i<rest->count;++i)if(used[i] && (!pose[i] || !Bounded(*pose[i]))){++stats.inputFallbacks;return nullptr;}
        Snapshot* result=available;
        if(result)available=result->nextFree;
        else{
            if(stats.allocated==MaximumSnapshots){++stats.allocationFailures;return nullptr;}
            result=new(std::nothrow)Snapshot;if(!result){++stats.allocationFailures;return nullptr;}
            result->nextAllocated=allocated;allocated=result;++stats.allocated;
        }
        result->pool=this;result->rest=rest;rest->Retain();result->references=1;result->view=view;
        std::memset(result->used,0,sizeof(result->used));result->ResetConsumer();
        for(unsigned i=0;i<rest->count;++i)if(used[i]){result->used[i]=true;result->pose[i]=*pose[i];}
        result->used[rest->count]=true;
        ++stats.captures;if(++stats.live>stats.peak)stats.peak=stats.live;return result;
    }
};
inline void Snapshot::Release()
{
    if(--references)return;
    pool->stats.computedBones+=computedCount;--pool->stats.live;
    rest->Release();rest=nullptr;nextFree=pool->available;pool->available=this;
}
struct Recipe {
    Snapshot* snapshot;
    unsigned count,first;
    unsigned char joints[picaSkin::MaxBones];
};
inline bool Execute(const void* bytes)
{
    Recipe recipe;std::memcpy(&recipe,bytes,sizeof(recipe));
    if(!recipe.snapshot || !recipe.count || recipe.count>picaSkin::MaxBones || recipe.first>96 || 3*recipe.count>96-recipe.first)return false;
    picaPreparedSkinPalette palette;palette.count=recipe.count;
    for(unsigned i=0;i<recipe.count;++i){
        if(!recipe.snapshot->Pack(recipe.joints[i]))return false;
        std::memcpy(palette.values[i],recipe.snapshot->packed[recipe.joints[i]],sizeof(palette.values[i]));
    }
    return picaUploadPreparedSkinPalette(palette,recipe.first);
}
}
#endif
