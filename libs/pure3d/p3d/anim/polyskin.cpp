/*===========================================================================
    polyskin.cpp
    30-Nov-99 Created by Neall
    12-Nov-00 Revamped for R14

    Copyright (c)1999, 2000 Radical Entertainment, Inc.
    All rights reserved.
===========================================================================*/

#include <p3d/anim/polyskin.hpp>
#ifdef SRR2_OLD3DS_SKIN_CLIP
#include <pddi/pica/skinclip.hpp>
#endif
#if defined(SRR2_OLD3DS_GPU_SKIN_BATCH) || defined(SRR2_OLD3DS_SKIN_CLIP)
#include <pddi/pica/context.hpp>
#include <typeinfo>
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_PEDESTRIANS
#include <pddi/pica/skinselection.hpp>
#endif
#include <p3d/shader.hpp>
#include <p3d/vertexlist.hpp>
#include <p3d/primgroup.hpp>
#include <constants/chunkids.hpp>
#include <p3d/chunkfile.hpp>
#include <p3d/utility.hpp>
#include <p3d/anim/skeleton.hpp>
#include <p3d/matrixstack.hpp>
#include <radmath/radmath.hpp>
#include <pddi/pddiext.hpp>
#include <p3d/anim/instancedgeometry.hpp>

#include <string.h>

using namespace RadicalMathLibrary;
#ifdef SRR2_OLD3DS_BONE_USAGE
#include <cstdio>
static unsigned boneUsageCalls=0,boneUsageComputed=0,boneUsageAvoided=0,boneUsageFallbacks=0;
extern "C" void old3dsDumpBoneUsage()
{
    printf("BONE_USAGE calls=%u computed=%u avoided=%u fallbacks=%u bytes_per_skin=%u\n",
        boneUsageCalls,boneUsageComputed,boneUsageAvoided,boneUsageFallbacks,(unsigned)sizeof(p3dSkin::BoneUsage));
}
#endif

static rmt::Matrix s_SkinMatrices[256]
#ifdef RAD_PS2
__attribute__((aligned(16)))
#endif
;

tPolySkin::tPolySkin(int npg) :
    boneMatrix(NULL),
    primGroup(npg),
    exprAnimOffsets(NULL)
{
    boundingSphere.centre.Set(0.0f, 0.0f, 0.0f);
    boundingSphere.radius = 1.0f;
    boundingBox.low.Set(0,0,0);
    boundingBox.high.Set(1,1,1);
}

tPolySkin::~tPolySkin()
{
//    delete[] boneMatrix;

    for(unsigned i=0; i < primGroup.Size(); i++)
    {
        primGroup[i]->Release();
    }
    
    tRefCounted::Release(skeleton);

    if(exprAnimOffsets)
    {
        exprAnimOffsets->Release();
    }
}


void tPolySkin::Display(tPose* p)
{
    if (p == NULL)
    {
        return;
    }

    // transform the pose joints into world space
    p->Evaluate();

    // The skin vertices are stored in rest-pose relative world space.
    // The below matrix will transform from this space, into bone-local
    // space where the animation is applied, and then back into camera space.
#ifdef SRR2_OLD3DS_BONE_USAGE
    const bool selectiveBones=boneUsage.Ready(p->GetNumJoint()) && !exprAnimOffsets;
    ++boneUsageCalls;
    if(!selectiveBones) ++boneUsageFallbacks;
#endif
    for(int i = 0; i < p->GetNumJoint(); i++)
    {
#ifdef SRR2_OLD3DS_BONE_USAGE
        if(selectiveBones && !boneUsage.Uses(i)){++boneUsageAvoided;continue;}
        ++boneUsageComputed;
#endif
#ifndef RAD_PS2
        boneMatrix[i].Mult(skeleton->GetJoint(i)->inverseWorldMatrix, p->GetJoint(i)->worldMatrix);
#else
        boneMatrix[i].MultAligned(skeleton->GetJoint(i)->inverseWorldMatrix, p->GetJoint(i)->worldMatrix);
#endif

#ifndef SRR2_OLD3DS_BONE_ONCE
        boneMatrix[i].Mult(skeleton->GetJoint(i)->inverseWorldMatrix, p->GetJoint(i)->worldMatrix);
#endif

    }

    boneMatrix[skeleton->GetNumJoint()].Identity();
#ifdef SRR2_OLD3DS_SKIN_CLIP
    // Preserve pose evaluation and all custom Display overrides. Exact PC skins
    // share one conservative bounds/transform check before their graphics work.
    const picaSkinClip::Bounds* clipGroups[32];
    bool clipComplete=primGroup.Size()>0 && primGroup.Size()<=32;
    for(unsigned i=0;clipComplete && i<primGroup.Size();++i) {
        auto* group=primGroup[i];
        if(!group || typeid(*group)!=typeid(tPrimGroupSkinnedPC)) {clipComplete=false;break;}
        clipGroups[i]=static_cast<picaPrimBuffer*>(static_cast<tPrimGroupSkinnedPC*>(group)->GetBuffer())->skinClipBounds;
        if(!clipGroups[i]) clipComplete=false;
    }
    if(clipComplete && static_cast<picaContext*>(p3d::pddi)->CullSkinBounds(clipGroups,primGroup.Size(),
        reinterpret_cast<const pddiMatrix*>(boneMatrix),skeleton->GetNumJoint()+1)) return;
#endif

    pddiExtHardwareSkinning* hwSkin = p3d::context->GetHardwareSkinning();

    if(hwSkin)
    {
        hwSkin->Begin();
    }

#ifdef SRR2_OLD3DS_GPU_SKIN_BATCH
    picaContext* skinContext = static_cast<picaContext*>(p3d::pddi);
    bool skinBatchOpen = false;
#endif
    for(unsigned j=0; j < primGroup.Size(); j++)
    {
#ifdef SRR2_OLD3DS_GPU_SKIN_BATCH
        // Only the exact CPU/GPU skin path participates. Other virtual drawables
        // retain a normal-program boundary before invoking their own behavior.
        auto* group = primGroup[j];
        const bool batchSafe = typeid(*group) == typeid(tPrimGroupSkinnedPC);
        if(!batchSafe && skinBatchOpen)
        {
            skinContext->EndSkinBatch();
            skinBatchOpen = false;
        }
        if(batchSafe && !skinBatchOpen)
        {
            skinContext->BeginSkinBatch();
            skinBatchOpen = true;
        }
#endif
        primGroup[j]->Display();
    }
#ifdef SRR2_OLD3DS_GPU_SKIN_BATCH
    if(skinBatchOpen) skinContext->EndSkinBatch();
#endif

    if(hwSkin)
    {
        hwSkin->End();
    }
}

void tPolySkin::DisplayInstanced(tPose* p, unsigned count)
{
    P3DASSERT(p);

    for(int i = 0; i < p->GetNumJoint(); i++)
    {
        boneMatrix[i] = p->GetJoint(i)->worldMatrix;
    }

    pddiExtHardwareSkinning* hwSkin = p3d::context->GetHardwareSkinning();

    if(hwSkin)
    {
        hwSkin->Begin();
    }

    for(unsigned j=0; j < primGroup.Size(); j++)
    {
        primGroup[j]->DisplayInstanced(count);
    }

    if(hwSkin)
    {
        hwSkin->End();
    }
}


void tPolySkin::ProcessShaders(ShaderCallback& callback)
{
    for(unsigned i = 0; i < primGroup.Size(); i++)
    {
        tShader* shader = primGroup[i]->GetShader();
        tShader* newShader = callback.Process(shader);
        if(newShader != shader)
        {
            primGroup[i]->SetShader(newShader);
        }
    }
}

//-------------------------------------------------------------------
static const int POLYSKIN_VERSION = 3;
static const int POLYSKIN_NONOPTIMISED_VERSION = 4;

tPolySkinLoader::tPolySkinLoader() : tSimpleChunkHandler(Pure3D::Mesh::SKIN), optimise(true) 
{
};

tEntity* tPolySkinLoader::LoadObject(tChunkFile* f, tEntityStore* store)
{
    char name[128];
    f->GetPString(name);
    
    int version = f->GetLong();
    P3DASSERT(version == POLYSKIN_VERSION || version == POLYSKIN_NONOPTIMISED_VERSION );

    optimise = version == POLYSKIN_VERSION;

    char skelName[128];
    f->GetPString(skelName);
    int nPrimGroup = f->GetLong();

    tPolySkin *skin = new tPolySkin(nPrimGroup);
    skin->SetName(name);

    skin->skeleton = p3d::find<tSkeleton>(store, skelName);
    if(skin->skeleton)
    {
        skin->skeleton->AddRef();

        P3DASSERT(skin->skeleton->GetNumJoint() < 256);

        // allocate one extra bone for the special identity matrix
        skin->boneMatrix = s_SkinMatrices;//new rmt::Matrix[skin->skeleton->GetNumJoint()+1];
    }
    int index = 0;
    bool deformed = false;

    while(f->ChunksRemaining())
    {
        f->BeginChunk();
        switch(f->GetCurrentID())
        {
            case Pure3D::Mesh::PRIMGROUP:
            {
                P3DASSERT(index < nPrimGroup);
                tPrimGroupLoader pgLoader;                
                skin->primGroup[index] = pgLoader.Load(f, store, skin->boneMatrix, optimise, true);
#ifdef SRR2_OLD3DS_GPU_SKIN_SUPPORT
                tPrimGroupSkinnedPC* pcSkin=dynamic_cast<tPrimGroupSkinnedPC*>(skin->primGroup[index]);
                bool selected=strcmp(name,"homer_h_merged")==0 && strcmp(skelName,"homer")==0;
#ifdef SRR2_OLD3DS_GPU_SKIN_PLAYERS
                selected=selected || (strcmp(name,"bart_h_merged")==0 && strcmp(skelName,"bart")==0);
#endif
                bool pedestrian = false;
#ifdef SRR2_OLD3DS_GPU_SKIN_PEDESTRIANS
                pedestrian = picaSkin::SelectPedestrian(name, skelName);
#endif
                bool named = false;
#ifdef SRR2_OLD3DS_GPU_SKIN_CAST
                selected = selected || picaSkin::SelectAdditionalPlayer(name, skelName);
                pedestrian = pedestrian || picaSkin::SelectAdditionalPedestrian(name, skelName);
                named = picaSkin::SelectNamedCharacter(name, skelName);
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_COSTUMES
                selected = selected || picaSkin::SelectCostumePlayer(name, skelName);
                pedestrian = pedestrian || picaSkin::SelectCostumePedestrian(name, skelName);
                named = named || picaSkin::SelectCostumeNamed(name, skelName);
#endif
#ifdef SRR2_OLD3DS_GPU_SKIN_PACKED
                selected = selected || picaSkin::SelectPackedPlayer(name, skelName);
                named = named || picaSkin::SelectPackedNamed(name, skelName);
#endif
                if(pcSkin) pcSkin->PrepareGpuSkin(selected || pedestrian || named, pedestrian, named);
#endif
                skin->primGroup[index]->AddRef();
                index++;
            }
            break;

            case Pure3D::Mesh::SPHERE:
                skin->boundingSphere.centre.x = f->GetFloat();
                skin->boundingSphere.centre.y = f->GetFloat();
                skin->boundingSphere.centre.z = f->GetFloat();
                skin->boundingSphere.radius = f->GetFloat();
            break;

            case Pure3D::Mesh::BOX:
                skin->boundingBox.low.x = f->GetFloat();
                skin->boundingBox.low.y = f->GetFloat();
                skin->boundingBox.low.z = f->GetFloat();
                skin->boundingBox.high.x = f->GetFloat();
                skin->boundingBox.high.y = f->GetFloat();
                skin->boundingBox.high.z = f->GetFloat();
            break;

            case Pure3D::Mesh::EXPRESSIONOFFSETS:
            {
                skin->exprAnimOffsets = dynamic_cast<tExpressionOffsets*> ( offsetLoader.LoadObject(f, store) );
                if(skin->exprAnimOffsets)
                {
                    (skin->exprAnimOffsets)->AddRef();
                }
                deformed = true;
            }
            break;

            default:
            break;
        }
        f->EndChunk();
    }

#ifdef SRR2_OLD3DS_BONE_USAGE
    // All consumers must be the exact known palette implementation. Unknown
    // groups and deformable skins retain the original complete matrix loop.
    if(skin->skeleton && !deformed)
    {
        skin->boneUsage.Begin(skin->skeleton->GetNumJoint());
        for(unsigned i=0;i<skin->primGroup.Size();++i)
        {
            tPrimGroup* group=skin->primGroup[i];
            if(!group || typeid(*group)!=typeid(tPrimGroupSkinnedPC) ||
               !static_cast<tPrimGroupSkinnedPC*>(group)->CollectBoneUsage(skin->boneUsage,skin->boneMatrix))
            {skin->boneUsage.Invalidate();break;}
        }
    }
#endif

    if(!deformed)
    {
        for(int i = 0; i < nPrimGroup; i++)
        {
            skin->primGroup[i]->ReleaseTempVertices();
        }
    }

#if defined(RAD_WIN32) || defined(RAD_XBOX)
    if( skin->primGroup[0]->GetInstanceCount())
    {
        tInstancedGeometry* inst = new tInstancedGeometry(skin);
        inst->SetName(name);
        char str[256];
        strcpy(str,name);
        strcat(str, "_inst");
        skin->SetName(str);
        return inst;
    }
#endif

    return skin;
}
