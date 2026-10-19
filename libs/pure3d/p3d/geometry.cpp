#ifdef SRR2_OLD3DS_MAIN_PROFILE
#include <main/old3dsmainprofile.h>
#else
#define O3_MAIN_SCOPE(stage) ((void)0)
#endif
//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#include <p3d/geometry.hpp>
#include <p3d/vertexlist.hpp>
#include <p3d/primgroup.hpp>
#include <p3d/chunkfile.hpp>
#include <p3d/file.hpp>
#include <constants/chunks.h>
#include <p3d/utility.hpp>
#include <pddi/pddiext.hpp>
#include <constants/chunkids.hpp>
#include <p3d/array.hpp>

#include <string.h>
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
#include <pddi/pica/context.hpp>
#include <p3d/shader.hpp>
#include <typeinfo>
#include <main/old3dsdiag.h>
#endif
#ifdef SRR2_OLD3DS_QUEUE_METRICS
#include <pddi/pica/queuemetrics.hpp>
#include <main/old3dsdiag.h>
#endif



using namespace RadicalMathLibrary;

//------------------------------------------------------------------------
tGeometry::tGeometry(int nPG) :
    //nFaceNormal(0),
    //faceNormal(NULL),
    primGroup( 0 )
{
    P3DASSERT(p3d::pddi);
    sphere.centre.x = 0.0f;
    sphere.centre.y = 0.0f;
    sphere.centre.z = 0.0f;
    sphere.radius = 1.0f;
    box.low.x = 0.0f;
    box.low.y = 0.0f;
    box.low.z = 0.0f;
    box.high.x = 0.0f;
    box.high.y = 0.0f;
    box.high.z = 0.0f;

    primGroup.SetSize( nPG );
}

//------------------------------------------------------------------------
tGeometry::~tGeometry()
{
//    delete[] faceNormal;


    for(unsigned i = 0; i < primGroup.Size(); i++)
    {
        tRefCounted::Release( primGroup[i] );
    }

}

//------------------------------------------------------------------------
tShader* tGeometry::GetShader(int i)
{ 
    if( primGroup[i])
        return primGroup[i]->GetShader(); 

    return NULL;
}

void tGeometry::SetShader(int i, tShader* shader) 
{ 
    if( primGroup[i] )
        primGroup[i]->SetShader(shader); 
}

void tGeometry::ProcessShaders(ShaderCallback& callback)
{
    for( unsigned i = 0; i < primGroup.Size(); i++)
    {
        tShader* shader = primGroup[i]->GetShader();
        tShader* newShader = callback.Process(shader);
        if(newShader != shader)
        {
            primGroup[i]->SetShader(newShader);
        }
    }
}

void tGeometry::SetPrimGroup(int i, tPrimGroup* group)
{
#ifdef SRR2_OLD3DS_VEHICLE_RENDER_BINDINGS
    // Saturation permanently disables prepared slot reuse for this geometry.
    if(shaderBindingRevision != ~0u) ++shaderBindingRevision;
#endif
    tRefCounted::Assign(primGroup[i], group);
}

//------------------------------------------------------------------------
void tGeometry::Display()
{
    O3_MAIN_SCOPE(Geometry);
#if defined(SRR2_OLD3DS_MATERIAL_QUEUE) || defined(SRR2_OLD3DS_QUEUE_METRICS)
    // Scope is deliberately inside Display: no entity, transform, light, pose,
    // texture mutation, or transient stream outlives its original call site.
    const bool staticWorld = (g_old3dsActiveEntity == O3_STATIC
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
        || static_cast<picaContext*>(p3d::pddi)->staticQueueActive
#endif
        ) && primGroup.Size() > 0;
#ifdef SRR2_OLD3DS_QUEUE_METRICS
    const bool sample = staticWorld && g_old3dsActiveEntity == O3_STATIC && ((++g_picaQueueMetrics.geometries & 31u) == 1u);
    if(sample) ++g_picaQueueMetrics.geometrySamples;
    picaQueueTimer geometryTimer(g_picaQueueMetrics.geometryTicks, sample);
#endif
#ifdef SRR2_OLD3DS_MATERIAL_QUEUE
    if(picaQueue::Enabled() && staticWorld && (primGroup.Size() > 1 || static_cast<picaContext*>(p3d::pddi)->staticQueueActive))
    {
        picaContext* context = static_cast<picaContext*>(p3d::pddi);
#ifndef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
        // With ordering disabled, consume packets immediately and retain only
        // compatible native state. Avoid staging/copying an unchanged draw order.
        const bool localScope = !context->staticQueueActive;
        if(localScope) context->BeginStaticQueue();
#else
        picaContext::Submission localEntries[picaQueue::Capacity];
        unsigned localCount = 0;
        picaContext::Submission* entries = context->staticQueueActive ? context->staticQueue : localEntries;
        unsigned& count = context->staticQueueActive ? context->staticQueueCount : localCount;
#endif
        for(unsigned i = 0; i < primGroup.Size(); ++i)
        {
            tPrimGroup* group = primGroup[i];
            if(!group) continue;
            // Exact type excludes skins, dynamic streamers and custom Display
            // overrides. Unsupported Display executes exactly once, in place.
            if(typeid(*group) == typeid(tPrimGroupOptimised))
            {
                tPrimGroupOptimised* optimised = static_cast<tPrimGroupOptimised*>(group);
                pddiShader* material = group->GetShader()->GetShader();
                if(context->CanQueue(material, optimised->GetBuffer()))
                {
#ifndef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
                    context->SubmitQueuedPrim(material, static_cast<picaPrimBuffer*>(optimised->GetBuffer()));
#else
                    entries[count].material = material;
                    entries[count].buffer = static_cast<picaPrimBuffer*>(optimised->GetBuffer());
                    entries[count].rect.valid = false;
                    if(++count == picaQueue::Capacity)
                    {
#ifdef SRR2_OLD3DS_QUEUE_METRICS
                        ++g_picaQueueMetrics.capacityFlushes;
#endif
                        context->SubmitMaterialQueue(entries, count);
                        count = 0;
                    }
#endif
                    continue;
                }
            }
#ifndef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
            context->staticQueuePrevious = NULL;
#else
            context->SubmitMaterialQueue(entries, count);
            count = 0;
#endif
#ifdef SRR2_OLD3DS_QUEUE_METRICS
            ++g_picaQueueMetrics.barriers;
#endif
            // Arbitrary Display can call nested geometry, then mutate its
            // transform or scratch storage. Never let that nested call borrow
            // the outer world's deferred lifetime.
            const bool sharedScope = context->staticQueueActive;
            context->staticQueueActive = false;
            group->Display();
            context->staticQueueActive = sharedScope;
        }
#ifndef SRR2_OLD3DS_MATERIAL_QUEUE_SORT
        if(localScope) context->EndStaticQueue();
#else
        if(!context->staticQueueActive) context->SubmitMaterialQueue(entries, count);
#endif
        return;
    }
#endif
#endif

    for( unsigned i = 0; i < primGroup.Size(); i++)
    {
        if( primGroup[i])
        {
            primGroup[i]->Display();
        }
    }
}
    
//------------------------------------------------------------------------
void tGeometry::SetBoundingBox(float x1, float y1, float z1,
                                         float x2, float y2, float z2)
{
    box.low.x = x1;
    box.low.y = y1;
    box.low.z = z1;
    box.high.x = x2;
    box.high.y = y2;
    box.high.z = z2;
}

//------------------------------------------------------------------------
void tGeometry::SetBoundingSphere(float centerx, float centery, float centerz,
                                             float radius)
{
    sphere.centre.x =  centerx;
    sphere.centre.y =  centery;
    sphere.centre.z =  centerz;
    sphere.radius   =  radius;
}

//------------------------------------------------------------------------
tGeometryLoader::tGeometryLoader() : 
    tSimpleChunkHandler(Pure3D::Mesh::MESH), 
    mEnableFaceNormals(false), 
    mOptimize(true), 
    mVertexMask(0xffffffff) { /**/ }


//------------------------------------------------------------------------
tGeometry* tGeometryLoader::Allocate(int nPrimGroup)
{
    return new tGeometry(nPrimGroup);
}

static const int GEOMETRY_VERSION = 0;
static const int GEOMETRY_NONOPTIMIZE_VERSION = 1;      //temporarily used

//------------------------------------------------------------------------
tEntity* tGeometryLoader::LoadObject(tChunkFile* f, tEntityStore* store)
{ 
    char name[255];
    f->GetPString(name);

    int version = f->GetLong();
    P3DASSERT(version == GEOMETRY_VERSION || version == GEOMETRY_NONOPTIMIZE_VERSION );

    bool bOptimized = ( version != GEOMETRY_NONOPTIMIZE_VERSION );

    int nPrimGroup = f->GetLong();
    tGeometry *geo = Allocate(nPrimGroup);
    geo->SetName(name);

    int primGroupCount = 0;

    while(f->ChunksRemaining())
    {      
        f->BeginChunk();
        switch(f->GetCurrentID())
        {
        
            case Pure3D::Mesh::PRIMGROUP:
            {
                P3DASSERT(primGroupCount < nPrimGroup);
                tPrimGroupLoader pgLoader;
                pgLoader.SetVertexFormatMask(mVertexMask);
                tPrimGroup *pg = pgLoader.Load(f, store, NULL, mOptimize && bOptimized, false);
                geo->SetPrimGroup( primGroupCount, pg );
                ++primGroupCount;
                
                break;
            }

/*****
            case P3D_FACE_NORMAL_LIST:
            {
                if(enableFaceNormals)
                {
                    geo->nFaceNormal = f->GetLong();
                    geo->faceNormal = new rmt::Vector[geo->nFaceNormal];
                    f->GetData(geo->faceNormal, geo->nFaceNormal * 3, tFile::DWORD);
                }
                break;
            }
*****/
            case Pure3D::Mesh::BOX:
            {
                float minx = f->GetFloat();
                float miny = f->GetFloat();
                float minz = f->GetFloat();
                float maxx = f->GetFloat();
                float maxy = f->GetFloat();
                float maxz = f->GetFloat();

                geo->SetBoundingBox( minx, miny, minz, maxx, maxy, maxz);
                break;
            }

            case Pure3D::Mesh::SPHERE:
            {
                float cx = f->GetFloat();
                float cy = f->GetFloat();
                float cz = f->GetFloat();
                float r  = f->GetFloat();

                geo->SetBoundingSphere(cx,cy,cz,r);
                break;
            }

            case Pure3D::Mesh::RENDERSTATUS:
            {
                geo->SetCastsShadow(!(f->GetLong()));
                break;
            }

            default:
                break;
        } // switch
        f->EndChunk();
    } // while

    return geo;
}

