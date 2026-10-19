#ifndef PICA_PREPARED_EXECUTOR_HPP
#define PICA_PREPARED_EXECUTOR_HPP
#include <main/old3dshot.h>
#include <pddi/pica/prepareddraw.hpp>
#include <pddi/pica/preparedgeometry.hpp>
#include <pddi/pica/nativecontext.h>
#include <pddi/pica/preparedstate.hpp>
#include <pddi/pica/preparedskin.hpp>
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
#include <pddi/pica/deferredskin.hpp>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>

// Ordered coarse execution stage. Only main runs it for now. The producer owns
// each input pool until Flush completes; a future worker must use the same lease
// and retain both operation storage and pool through its completion handoff.
class picaPreparedExecutor {
public:
    static constexpr unsigned Capacity=32,DrawWordBound=2048;
    picaPreparedUniformCache uniforms;
    picaStateCache state;
    bool valid=false,draining=false;
    unsigned operations=0,geometry=0,flushes=0,submitted=0,paletteCount=0;
    unsigned stateChanges=0,textureRequests=0,textureChanges=0;
    using DriverCall=void(*)(const void*);
    unsigned driverUsed=0;
    bool DriverRoom(unsigned size)const{return !Full() && size<=sizeof(driverBytes) && ((driverUsed+15u)&~15u)<=sizeof(driverBytes)-size;}
    // Only leaf driver operations with explicitly owned, trivial inputs belong
    // here. Never defer game callbacks or retain pointers to transient inputs.
    void AddDriver(DriverCall call,const void* data,unsigned size)
    {
        Require(valid && call && DriverRoom(size),"driver_record_capacity");
        driverUsed=(driverUsed+15u)&~15u;
        Operation& operation=items[operations++];operation.kind=Driver;
        operation.driver=call;operation.first=driverUsed;
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
        operation.deferredSkin=nullptr;operation.driverTry=nullptr;
#endif
        memcpy(driverBytes+driverUsed,data,size);driverUsed+=size;
    }
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
    void AddDeferredSkin(const picaDeferredSkin::Recipe& input)
    {
        AddDriver(nullptrDriver,&input,sizeof(input));
        auto& operation=items[operations-1];operation.driverTry=picaDeferredSkin::Execute;
        operation.deferredSkin=input.snapshot;input.snapshot->Retain();ownedOperations=operations;
    }
    void ResetDeferredSkinCaches()
    {
        for(unsigned i=0;i<ownedOperations;++i)if(items[i].kind==Driver && items[i].deferredSkin)items[i].deferredSkin->ResetConsumer();
    }
#endif
    void Begin()
    {
        Require(!valid && !operations,"begin_while_owned");
        flushes=submitted=stateChanges=textureRequests=textureChanges=0;
        uniforms.modelViewUploads=uniforms.modelViewReuses=uniforms.fastUnlitCalls=uniforms.otherProjectionCalls=uniforms.directionalDraws=uniforms.generalLightDraws=0;
        state=g_picaState;
        originalTexture=picaNativeTexture0();
        if(originalTexture){memcpy(&texture,originalTexture,sizeof(texture));picaNativeTexture0Alias(&texture);}
        valid=true;
    }
    // Only the SDK consumer transfers these fields. Input arenas are immutable
    // after publication; producer capture never touches the consumer cache.
    void AdoptConsumer(const picaPreparedExecutor& previous)
    {
        if(this==&previous)return;
        uniforms=previous.uniforms;state=previous.state;texture=previous.texture;
        originalTexture=previous.originalTexture;valid=true;
        flushes=previous.flushes;submitted=previous.submitted;
        stateChanges=previous.stateChanges;textureRequests=previous.textureRequests;textureChanges=previous.textureChanges;
        if(picaNativeTexture0())picaNativeTexture0Alias(&texture);
    }
    bool Full() const{return operations==Capacity;}
    bool HasRoomForGeometry() const
    {
        u32* buffer;u32 size,offset;GPUCMD_GetBuffer(&buffer,&size,&offset);
        const unsigned bound=DrawWordBound*(geometry+1);
        return buffer && offset<=size && bound<=size-offset;
    }
    void AddMaterial(const picaPreparedDrawInputs& input,const picaPreparedInputPool& pool)
    {
        Require(valid && !Full(),"material_capacity");
        Operation& operation=items[operations++];operation.kind=Material;
        new (&operation.material) MaterialOperation;
        operation.material.pool=&pool;
        operation.material.input.hasMaterial=input.hasMaterial;operation.material.input.hasUniforms=input.hasUniforms;
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
        operation.material.input.retained=input.retained;
        if(input.retained){input.retained->Retain();ownedOperations=operations;}
#endif
        if(input.hasMaterial){
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
            if(!input.retained)
#endif
            memcpy(&operation.material.input.material,&input.material,sizeof(input.material));
            operation.material.input.raster=input.raster;
        }
        if(input.hasUniforms)operation.material.input.uniforms=input.uniforms;
    }
#if defined(SRR2_OLD3DS_RETAINED_MATERIAL) || defined(SRR2_OLD3DS_DEFERRED_SKIN)
    // Call only on main after the consumer has completed, including error paths.
    void ReleaseOwnedInputs()
    {
        for(unsigned i=0;i<ownedOperations;++i){
            auto& operation=items[i];
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
            if(operation.kind==Material && operation.material.input.retained){operation.material.input.retained->Release();operation.material.input.retained=nullptr;}
#endif
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
            if(operation.kind==Driver && operation.deferredSkin){operation.deferredSkin->Release();operation.deferredSkin=nullptr;}
#endif
        }
        ownedOperations=0;
    }
#endif

    void AddGeometry(const picaPreparedGeometry& input)
    {
        Require(valid && !Full(),"geometry_capacity");
        Operation& operation=items[operations++];operation.kind=Geometry;new (&operation.geometry) picaPreparedGeometry(input);++geometry;
    }
    bool PaletteFull()const{return paletteCount==PaletteCapacity;}
    void AddPalette(const picaPreparedSkinPalette& input,unsigned first)
    {
        Require(valid && !Full() && !PaletteFull() && input.count && input.count<=picaSkin::MaxBones,"palette_capacity");
        picaPreparedSkinPalette& target=palettes[paletteCount];target.count=input.count;
        memcpy(target.values,input.values,input.count*sizeof(input.values[0]));
        Operation& operation=items[operations++];operation.kind=Palette;operation.palette=paletteCount++;operation.first=first;
    }
    void AddProgram(shaderProgram_s* program,C3D_AttrInfo* attributes,bool resetEye,unsigned eyeRegister,float convergence)
    {
        Require(valid && !Full(),"program_capacity");
        Operation& operation=items[operations++];operation.kind=Program;operation.program=program;operation.attributes=attributes;
        operation.normal=resetEye;operation.first=eyeRegister;operation.convergence=convergence;
    }
    void AddLayout(C3D_AttrInfo* attributes,bool fixedNormal)
    {
        Require(valid && !Full(),"layout_capacity");
        Operation& operation=items[operations++];operation.kind=Layout;operation.attributes=attributes;operation.normal=fixedNormal;
    }
    void AddState(const picaPreparedState& input)
    {
        Require(valid && !Full(),"state_capacity");
        Operation& operation=items[operations++];operation.kind=State;new (&operation.state) picaPreparedState(input);
    }
    const char* failureReason=nullptr;
    OLD3DS_HOT_PATH bool TryFlush()
    {
        failureReason=nullptr;
        if(!operations)return true;
        if(!valid || draining){failureReason="recursive_execution";return false;}
        draining=true;++flushes;
        for(unsigned i=0;i<operations;++i)
        {
            Operation& operation=items[i];
            if(operation.kind==Material)
            {
                picaPreparedDrawInputs& input=operation.material.input;
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
                const auto* retained=input.retained;
#endif
                if(input.hasMaterial)
                {
                    picaPreparedMaterialCounters counters;
                    const picaPreparedMaterialPass& pass=
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
                        retained?retained->pass:
#endif
                        input.material;
                    picaApplyPreparedMaterialPass(pass,state,counters,nullptr,&texture);
                    if(pass.textureIdentity)originalTexture=const_cast<C3D_Tex*>(pass.textureIdentity);
                    stateChanges+=counters.stateChanges+picaApplyPreparedMaterialRaster(input.raster,state);
                    textureRequests+=counters.textureRequests;textureChanges+=counters.textureChanges;
                }
                if(input.hasUniforms)picaApplyPreparedUniforms(operation.material.pool->View(input.uniforms
#ifdef SRR2_OLD3DS_RETAINED_MATERIAL
                    ,retained?&retained->uniforms:nullptr
#endif
                    ),uniforms);
            }
            else if(operation.kind==Program)
            {
                C3D_BindProgram(operation.program);
                if(operation.normal)C3D_FVUnifSet(GPU_VERTEX_SHADER,operation.first,0.0f,operation.convergence,0.0f,0.0f);
                C3D_SetAttrInfo(operation.attributes);state.bufInfoSet=false;uniforms.Invalidate();
            }
            else if(operation.kind==Layout)
            {
                C3D_SetAttrInfo(operation.attributes);
                if(operation.normal)C3D_FixedAttribSet(1,0.0f,0.0f,1.0f,0.0f);
                state.bufInfoSet=false;
            }
            else if(operation.kind==Palette){if(!picaUploadPreparedSkinPalette(palettes[operation.palette],operation.first)){failureReason="palette_register_bounds";draining=false;return false;}}
            else if(operation.kind==State)stateChanges+=picaApplyPreparedState(operation.state,state);
            else if(operation.kind==Driver){
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
                if(operation.driverTry){if(!operation.driverTry(driverBytes+operation.first)){failureReason="deferred_skin_input";draining=false;return false;}}
                else
#endif
                operation.driver(driverBytes+operation.first);
            }
            else
            {
                u32* buffer;u32 size,offset;GPUCMD_GetBuffer(&buffer,&size,&offset);
                if(!buffer || offset>size || DrawWordBound>size-offset){failureReason="geometry_command_room";draining=false;return false;}
                picaBindPreparedGeometry(operation.geometry,state.bufInfoSet);
                picaSubmitPreparedGeometry(operation.geometry);++submitted;
            }
        }
        operations=geometry=paletteCount=driverUsed=0;draining=false;return true;
    }
    OLD3DS_HOT_PATH void Flush(){const bool ok=TryFlush();Require(ok,failureReason?failureReason:"prepared_execution_failed");}
    void End()
    {
        if(!valid)return;Flush();
        // Restoring an SDK alias must not dirty a register or emit another bind.
        // All producer sampler mutations have already happened in original order.
        picaNativeTexture0Alias(originalTexture);
        g_picaState=state;valid=false;
    }
    static void Require(bool condition,const char* reason)
    {
        if(condition)return;
        fprintf(stderr,"PREPARED_EXECUTOR_INVARIANT reason=%s\n",reason);fflush(stderr);
        fprintf(stdout,"PREPARED_EXECUTOR_INVARIANT reason=%s\n",reason);fflush(stdout);std::exit(1);
    }
private:
#if defined(SRR2_OLD3DS_RETAINED_MATERIAL) || defined(SRR2_OLD3DS_DEFERRED_SKIN)
    unsigned ownedOperations=0;
#endif
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
    static void nullptrDriver(const void*){}
#endif
    enum Kind {Material,Geometry,State,Program,Layout,Palette,Driver};
    struct MaterialOperation {
        const picaPreparedInputPool* pool;
        picaPreparedDrawInputs input;
    };
    struct Operation {
        Kind kind;
        // A command consumes exactly one payload. Sharing its storage avoids
        // pulling unused geometry/state/material data through the CPU caches.
        // Placement construction in the writers starts the selected lifetime;
        // retained references are still released explicitly after consumer join.
        union {
            MaterialOperation material;
            picaPreparedGeometry geometry;
            picaPreparedState state;
        };
        Operation() {}
        shaderProgram_s* program;C3D_AttrInfo* attributes;
        unsigned palette,first;float convergence;bool normal;
        DriverCall driver;
#ifdef SRR2_OLD3DS_DEFERRED_SKIN
        picaDeferredSkin::Snapshot* deferredSkin;
        bool (*driverTry)(const void*);
#endif
    };
    static_assert(std::is_trivially_copyable<Operation>::value,
                  "Frame validation snapshots require trivial command copies");
    static_assert(std::is_trivially_destructible<Operation>::value,
                  "Command references retire explicitly after consumer completion");
    static constexpr unsigned PaletteCapacity=8;
    Operation items[Capacity];
    picaPreparedSkinPalette palettes[PaletteCapacity];
    alignas(16) unsigned char driverBytes[2048];
    C3D_Tex texture;
    C3D_Tex* originalTexture=nullptr;
};
static_assert(sizeof(picaPreparedExecutor)<=32768,"Bounded ordered backend operation arena");
#endif
