#ifndef PICA_PREPARED_EXECUTOR_HPP
#define PICA_PREPARED_EXECUTOR_HPP
#include <main/old3dshot.h>
#include <pddi/pica/prepareddraw.hpp>
#include <pddi/pica/preparedgeometry.hpp>
#include <pddi/pica/nativecontext.h>
#include <pddi/pica/preparedstate.hpp>
#include <pddi/pica/preparedskin.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
        operation.pool=&pool;
        operation.material.hasMaterial=input.hasMaterial;operation.material.hasUniforms=input.hasUniforms;
        if(input.hasMaterial){memcpy(&operation.material.material,&input.material,sizeof(input.material));operation.material.raster=input.raster;}
        if(input.hasUniforms)operation.material.uniforms=input.uniforms;
    }
    void AddGeometry(const picaPreparedGeometry& input)
    {
        Require(valid && !Full(),"geometry_capacity");
        Operation& operation=items[operations++];operation.kind=Geometry;operation.geometry=input;++geometry;
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
        Operation& operation=items[operations++];operation.kind=State;operation.state=input;
    }
    OLD3DS_HOT_PATH void Flush()
    {
        if(!operations)return;
        Require(valid && !draining,"recursive_execution");draining=true;++flushes;
        for(unsigned i=0;i<operations;++i)
        {
            Operation& operation=items[i];
            if(operation.kind==Material)
            {
                picaPreparedDrawInputs& input=operation.material;
                if(input.hasMaterial)
                {
                    picaPreparedMaterialCounters counters;
                    picaApplyPreparedMaterialPass(input.material,state,counters,nullptr,&texture);
                    if(input.material.textureIdentity)originalTexture=const_cast<C3D_Tex*>(input.material.textureIdentity);
                    stateChanges+=counters.stateChanges+picaApplyPreparedMaterialRaster(input.raster,state);
                    textureRequests+=counters.textureRequests;textureChanges+=counters.textureChanges;
                }
                if(input.hasUniforms)picaApplyPreparedUniforms(operation.pool->View(input.uniforms),uniforms);
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
            else if(operation.kind==Palette)Require(picaUploadPreparedSkinPalette(palettes[operation.palette],operation.first),"palette_register_bounds");
            else if(operation.kind==State)stateChanges+=picaApplyPreparedState(operation.state,state);
            else
            {
                u32* buffer;u32 size,offset;GPUCMD_GetBuffer(&buffer,&size,&offset);
                Require(buffer && offset<=size && DrawWordBound<=size-offset,"geometry_command_room");
                picaBindPreparedGeometry(operation.geometry,state.bufInfoSet);
                picaSubmitPreparedGeometry(operation.geometry);++submitted;
            }
        }
        operations=geometry=paletteCount=0;draining=false;
    }
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
    enum Kind {Material,Geometry,State,Program,Layout,Palette};
    struct Operation {
        Kind kind;const picaPreparedInputPool* pool;
        picaPreparedDrawInputs material;
        picaPreparedGeometry geometry;
        picaPreparedState state;
        shaderProgram_s* program;C3D_AttrInfo* attributes;
        unsigned palette,first;float convergence;bool normal;
    };
    static constexpr unsigned PaletteCapacity=8;
    Operation items[Capacity];
    picaPreparedSkinPalette palettes[PaletteCapacity];
    C3D_Tex texture;
    C3D_Tex* originalTexture=nullptr;
};
static_assert(sizeof(picaPreparedExecutor)<=32768,"Bounded ordered backend operation arena");
#endif
