#include <main/old3dspools.h>
#ifdef SRR2_OLD3DS_MAIN_PROFILE
#include <main/old3dsmainprofile.h>
#else
#define O3_MAIN_SCOPE(stage) ((void)0)
#endif
/*===========================================================================
    compositedrawable.hpp
    30-Oct-00 Created by Liberty 

    Copyright (c)2000 Radical Entertainment, Inc.
    All rights reserved.
===========================================================================*/

#include <p3d/billboardobject.hpp>
#include <p3d/anim/compositedrawable.hpp>
#include <p3d/anim/skeleton.hpp>
#include <p3d/anim/polyskin.hpp>
#include <p3d/anim/pose.hpp>
#include <p3d/anim/skeleton.hpp>
#include <p3d/utility.hpp>
#include <p3d/matrixstack.hpp>
#include <p3d/view.hpp>
#include <p3d/camera.hpp>
#include <p3d/chunkfile.hpp>
#include <p3d/file.hpp>
#include <constants/chunks.h>
#include <radmath/util.hpp>
#ifdef SRR2_OLD3DS_WHEEL_LOD
#include <p3d/geometry.hpp>
#include <p3d/primgroup.hpp>
#include <p3d/shader.hpp>
#include <pddi/pica/context.hpp>
#include <pddi/pica/wheellod.hpp>
#include <pddi/pica/material.hpp>
#include <typeinfo>
#include <cstdio>
#endif
#include <float.h>
#include <string.h>

#ifdef SRR2_OLD3DS_COARSE_POOLS
#include <p3d/geometry.hpp>
#include <p3d/primgroup.hpp>
#include <typeinfo>
static bool RecipeProp(tDrawable* drawable)
{
    if(!drawable || typeid(*drawable)!=typeid(tGeometry)) return false;
    tGeometry* geometry=static_cast<tGeometry*>(drawable);
    for(int i=0;i<geometry->GetNumPrimGroup();++i)
    {
        tPrimGroup* group=geometry->GetPrimGroup(i);
        if(group && typeid(*group)!=typeid(tPrimGroupOptimised))return false;
    }
    return true;
}
#endif
const int resizeElementSize = 2;
const int defaultInitialElementSize = 5;

tCompositeDrawable::tCompositeDrawable() :
    billboard(false)
{
    elements.SetSize( defaultInitialElementSize );

    boundingBox.low.Set(0,0,0);
    boundingBox.high.Set(1,1,1);
    boundingSphere.centre.Set(0,0,0);
    boundingSphere.radius = 1;
}

tCompositeDrawable::tCompositeDrawable(int initialElementSize) :
    billboard(false)
{
    elements.SetSize( initialElementSize );

    boundingBox.low.Set(0,0,0);
    boundingBox.high.Set(1,1,1);
    boundingSphere.centre.Set(0,0,0);
    boundingSphere.radius = 1;

    translucentObjects.SetSize(initialElementSize);
}

tCompositeDrawable::~tCompositeDrawable()
{
#ifdef SRR2_OLD3DS_WHEEL_LOD
    if(wheelLodBuffer) wheelLodBuffer->Release();
    if(wheelLodSource) wheelLodSource->Release();
    if(wheelLodDrawable) wheelLodDrawable->Release();
#endif
    for( unsigned i=0; i< elements.Size( ); i++)
    {
        if( elements[ i ] )
            delete elements[i];
    }

    if(skeleton)
    {
        skeleton->Release();
    }
}

tCompositeDrawable* tCompositeDrawable::Clone(tPose* pose)
{
    tCompositeDrawable* compDraw = new tCompositeDrawable( elements.Size( ) );
    compDraw->SetNameObject(GetNameObject());
    if ( pose == NULL )
        compDraw->SetPose(new tPose(skeleton));
    else
        compDraw->SetPose(pose);

    compDraw->EnableBillboard(billboard);

    //
    // Attach the skeleton and release the old one
    //
    tRefCounted::Assign(compDraw->skeleton,skeleton);
            
    //
    // Add Drawable Elements
    //
    for( unsigned i=0; i< elements.Size( ); i++)
    {
        if( elements[ i ] ){
            switch (elements[i]->GetType())
            {
                case DrawableElement::PROP_ELEMENT:
                {
                    DrawablePropElement* prop = (DrawablePropElement*)(elements[i]);
                    DrawablePropElement* newProp = compDraw->AddProp(prop->GetDrawable(), prop->poseIndex);
                    newProp->SetIsTranslucent(prop->IsTranslucent());
                    newProp->SetSortOrder(prop->SortOrder());
                    break;
                }
                case DrawableElement::SKIN_ELEMENT:
                {
                    DrawablePoseElement* pose = (DrawablePoseElement*)(elements[i]);
                    DrawablePoseElement* newPose = compDraw->AddPose((tDrawablePose*)pose->GetDrawable());
                    newPose->SetIsTranslucent(pose->IsTranslucent());
                    newPose->SetSortOrder(pose->SortOrder());
                    break;
                }
                case DrawableElement::EFFECT_ELEMENT:
                {
                    DrawableEffectElement* effect = (DrawableEffectElement*)(elements[i]);
                    DrawableEffectElement* newEffect = compDraw->AddEffect((tEffect*)effect->GetDrawable(), effect->poseIndex);
                    newEffect->SetIsTranslucent(effect->IsTranslucent());
                    newEffect->SetSortOrder(effect->SortOrder());
                    break;
                }
                default:
                {
                    P3DASSERTMSG(0, "Invalid DrawableElement", "tCompositeDrawable::Copy()");
                    break;
                }
            }
        }
    }

    //
    // Copy the Bounding Boxes
    //
    compDraw->boundingBox.high = boundingBox.high;
    compDraw->boundingBox.low = boundingBox.low;
    compDraw->boundingSphere.centre = boundingSphere.centre;
    compDraw->boundingSphere.radius = boundingSphere.radius;

    //
    // Size the Display list
    //
    compDraw->translucentObjects.SetSize( elements.Size( ) );

#ifdef SRR2_OLD3DS_WHEEL_LOD
    // Immutable baked geometry is shared. Distance hysteresis belongs to each car.
    compDraw->wheelLodAttempted=wheelLodAttempted;
    compDraw->wheelLodFirst=wheelLodFirst;compDraw->wheelLodVersion=wheelLodVersion;
    compDraw->wheelLodRadius=wheelLodRadius;compDraw->wheelLodExtent=wheelLodExtent;
    compDraw->wheelLodJoints=wheelLodJoints;compDraw->wheelLodDrawable=wheelLodDrawable;
    memcpy(compDraw->wheelLodPose,wheelLodPose,sizeof(wheelLodPose));
    memcpy(compDraw->wheelLodCenters,wheelLodCenters,sizeof(wheelLodCenters));
    memcpy(compDraw->wheelLodAxes,wheelLodAxes,sizeof(wheelLodAxes));
    compDraw->wheelLodBuffer=wheelLodBuffer;compDraw->wheelLodSource=wheelLodSource;
    if(wheelLodBuffer) wheelLodBuffer->AddRef();
    if(wheelLodSource) wheelLodSource->AddRef();
    if(wheelLodDrawable) wheelLodDrawable->AddRef();
#endif
    return compDraw;
}

//
// Effectively all we are doing here is 
// maintaining a collection of different drawables
// that can display themselves. The hierarchical 
// drawables used to take a "tpose" to draw... 
//
#ifdef SRR2_OLD3DS_WHEEL_LOD
static unsigned wheelLodPrepared=0,wheelLodBatches=0,wheelLodRequests=0,wheelLodPoseFallbacks=0;
static unsigned wheelLodAttempts=0,wheelLodGeometry=0,wheelLodGroups=0,wheelLodBounds=0,wheelLodCompatible=0;
extern "C" void old3dsDumpWheelLOD()
{
    printf("WHEEL_LOD prepared=%u batches=%u avoided_draw_requests=%u pose_fallbacks=%u\n",
        wheelLodPrepared,wheelLodBatches,wheelLodRequests,wheelLodPoseFallbacks);
    printf("WHEEL_PREP attempts=%u geometry=%u groups=%u bounds=%u compatible=%u\n",
        wheelLodAttempts,wheelLodGeometry,wheelLodGroups,wheelLodBounds,wheelLodCompatible);
}
void tCompositeDrawable::PrepareWheelLOD()
{
    // Called on vehicle masters before Clone, never during a render callback.
    if(wheelLodAttempted) return;
    wheelLodAttempted=true;++wheelLodAttempts;
    if(!skeleton || billboard || skeleton->GetNumJoint()<1) return;
    for(unsigned first=0;first+4<=elements.Size();++first)
    {
        DrawableElement* element=elements[first];
        if(!element || typeid(*element)!=typeid(DrawablePropElement) || element->IsTranslucent()) continue;
        tDrawable* drawable=element->GetDrawable();
        if(!drawable || typeid(*drawable)!=typeid(tGeometry)) continue;
        // Release builds retain UIDs but discard name text. These four wheel
        // names cover the audited car assets; unknown art keeps ordinary draws.
        static const tUID wheelNames[]={tEntity::MakeUID("wShape0"),tEntity::MakeUID("wShape1"),
                                        tEntity::MakeUID("wShape2"),tEntity::MakeUID("wShape3")};
        const tUID uid=drawable->GetUID();
        if(uid!=wheelNames[0] && uid!=wheelNames[1] && uid!=wheelNames[2] && uid!=wheelNames[3]) continue;
        ++wheelLodGeometry;
        tGeometry* geometry=static_cast<tGeometry*>(drawable);
        if(geometry->GetNumPrimGroup()!=1) continue;
        tPrimGroup* group=geometry->GetPrimGroup(0);
        if(!group || typeid(*group)!=typeid(tPrimGroupOptimised)) continue;
        ++wheelLodGroups;
        rmt::Sphere sphere;rmt::Box3D box;geometry->GetBoundingSphere(&sphere);geometry->GetBoundingBox(&box);
        const rmt::Vector span=box.high-box.low;
        // Small cylindrical wheels only; avoid large or differently oriented art.
        if(!std::isfinite(sphere.radius) || sphere.radius<=0 || sphere.radius>.65f ||
           !(span.x<.75f*span.y && span.x<.75f*span.z)) continue;
        ++wheelLodBounds;
        bool compatible=true;rmt::Matrix rest[4];
        for(unsigned i=0;i<4;++i)
        {
            DrawableElement* next=elements[first+i];
            if(!next || typeid(*next)!=typeid(DrawablePropElement) || next->IsTranslucent() || next->GetDrawable()!=drawable)
            {compatible=false;break;}
            const int index=static_cast<DrawablePropElement*>(next)->GetPoseIndex();
            if(index<0 || index>=skeleton->GetNumJoint()){compatible=false;break;}
            wheelLodPose[i]=index;
            rest[i].Mult(skeleton->GetJoint(index)->worldMatrix,skeleton->GetJoint(0)->inverseWorldMatrix);
        }
        if(!compatible) continue;
        ++wheelLodCompatible;
        picaPrimBuffer* source=static_cast<picaPrimBuffer*>(static_cast<tPrimGroupOptimised*>(group)->GetBuffer());
        if(!source || source->WheelSourceVersion()==~0u) return;
        picaPrimBuffer* baked=source->BakeWheelCopies(rest,4);
        if(!baked) return;
        for(unsigned i=0;i<4;++i)
        {
            memcpy(wheelLodCenters[i],&rest[i].Row(3),sizeof(wheelLodCenters[i]));
            memcpy(wheelLodAxes[i],&rest[i].Row(0),sizeof(wheelLodAxes[i]));
        }
        wheelLodRadius=sphere.radius;
        wheelLodExtent=sphere.radius;
        for(unsigned i=0;i<4;++i)
            wheelLodExtent=std::fmax(wheelLodExtent,rest[i].Row(3).Magnitude()+sphere.radius+sphere.centre.Magnitude());
        wheelLodBuffer=baked;baked->AddRef();wheelLodSource=source;source->AddRef();
        wheelLodVersion=source->WheelSourceVersion();wheelLodFirst=first;
        wheelLodDrawable=drawable;drawable->AddRef();wheelLodJoints=skeleton->GetNumJoint();++wheelLodPrepared;
        return;
    }
}
bool tCompositeDrawable::DrawWheelLOD(tPose* p)
{
    if(!wheelLodBuffer || !g_picaInFrame || billboard || p->GetNumJoint()!=int(wheelLodJoints) ||
       wheelLodSource->WheelSourceVersion()!=wheelLodVersion || wheelLodVersion==~0u ||
       wheelLodFirst+4>elements.Size()) return false;
#ifndef SRR2_OLD3DS_WHEEL_SKIN_BATCH
    const rmt::Matrix& root=p->GetJoint(0)->worldMatrix;
    rmt::Matrix modelView;modelView.Mult(root,*p3d::stack->GetMatrix());
    const rmt::Vector& cameraPosition=modelView.Row(3);
    wheelLodFar=picaWheelLOD::WantFar(wheelLodFar,cameraPosition.MagnitudeSqr());
    if(!wheelLodFar) return false;
    float view[4][4];memcpy(view,&modelView,sizeof(view));
    if(!picaWheelLOD::SmallEnough(static_cast<picaContext*>(p3d::pddi)->WheelLodPixelScale(),
                                 wheelLodRadius,wheelLodExtent,view)) return false;
#endif
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
    pddiMatrix wheels[4];
#else
    float wheels[4][4][4];
#endif
    for(unsigned i=0;i<4;++i)
    {
        DrawableElement* element=elements[wheelLodFirst+i];
        if(!element || typeid(*element)!=typeid(DrawablePropElement) || !element->IsVisible() ||
           element->IsTranslucent() || element->GetDrawable()!=wheelLodDrawable ||
           static_cast<DrawablePropElement*>(element)->GetPoseIndex()!=int(wheelLodPose[i])) return false;
        memcpy(&wheels[i],&p->GetJoint(wheelLodPose[i])->worldMatrix,sizeof(wheels[i]));
    }
    tGeometry* geometry=static_cast<tGeometry*>(wheelLodDrawable);
    if(geometry->GetNumPrimGroup()!=1) return false;
    tPrimGroup* group=geometry->GetPrimGroup(0);
    if(!group || typeid(*group)!=typeid(tPrimGroupOptimised) ||
       static_cast<tPrimGroupOptimised*>(group)->GetBuffer()!=wheelLodSource) return false;
    if(!group->GetShader() || !group->GetShader()->GetShader()) return false;
#ifdef SRR2_OLD3DS_WHEEL_SKIN_BATCH
    if(!static_cast<picaContext*>(p3d::pddi)->DrawWheelSkin(group->GetShader()->GetShader(),wheelLodBuffer,
            wheels)) return false;
#else
    if(!static_cast<picaMat*>(group->GetShader()->GetShader())->WheelLodOpaque()) return false;
    float matrix[4][4];memcpy(matrix,&root,sizeof(matrix));
    if(!picaWheelLOD::PoseClose(matrix,wheelLodCenters,wheelLodAxes,wheels))
    {++wheelLodPoseFallbacks;return false;}
    // Only exact, adjacent geometry callbacks are replaced. SetPose still runs
    // for every element, and all other effects, visibility and simulation stay live.
    p3d::stack->PushMultiply(root);
    p3d::pddi->DrawPrimBuffer(group->GetShader()->GetShader(),wheelLodBuffer);
    p3d::stack->Pop();
#endif
    ++wheelLodBatches;wheelLodRequests+=3;
    return true;
}
#endif

void tCompositeDrawable::Display(tPose* p)
{
    O3_POOL(Recipe,Upstream);
    O3_MAIN_SCOPE(Composite);
    if (billboard)
    {
        rmt::Matrix worldMatrix = *(p3d::context->GetWorldMatrix());
        rmt::Matrix cameraMatrix = p3d::context->GetView()->GetCamera()->GetCameraToWorldMatrix();

        rmt::Vector normal;
        normal.Sub(cameraMatrix.Row(3),worldMatrix.Row(3));
        normal.NormalizeSafe();

        rmt::Matrix transform;
        transform.Identity();
        transform.FillHeadingXZ(normal);

        p3d::stack->PushMultiply(transform);
    }

    //
    // Transform the pose joints into world space
    //
    { O3_POOL(Recipe,Pose); p->Evaluate(); }
    
    //
    // Display elements
    //
#ifdef SRR2_OLD3DS_WHEEL_LOD
    bool wheelLodDrawn=false;
#endif
    for( unsigned i=0; i< elements.Size( ); i++)
    {
        if(elements[i])
        {
            elements[i]->SetPose(p);
            if (elements[i]->IsVisible())
            {
#ifdef SRR2_OLD3DS_WHEEL_LOD
                if(wheelLodDrawn && i>wheelLodFirst && i<wheelLodFirst+4) continue;
                if(wheelLodBuffer && i==wheelLodFirst && DrawWheelLOD(p)){wheelLodDrawn=true;continue;}
#endif
                if (elements[i]->IsTranslucent())
                {
                    //if( BillboardQuadManager::sEnabled )    
                    //    elements[i]->Display();
                    //else 
                        translucentObjects.Add(elements[i]->GetDrawable(), elements[i]->GetWorldMatrix(),elements[i]->SortOrder());            
                }
                else
                {
                    elements[i]->Display();
                }
            }
        }
    }

    //if( !BillboardQuadManager::sEnabled )
        { O3_POOL(Recipe,Fallback); translucentObjects.Display(); }

    if (billboard)
    {
        p3d::stack->Pop();
    }
}


void tCompositeDrawable::ProcessShaders(ShaderCallback& callback)
{
    for( unsigned i=0; i< elements.Size( ); i++)
    {
        if(elements[i])
        {
            elements[i]->GetDrawable()->ProcessShaders(callback);
        }
    }
}

void tCompositeDrawable::SetPose(tPose* p)
{
    tRefCounted::Assign(pose, p);
    for( unsigned i=0; i< elements.Size( ); i++)
    {
        if(elements[i])
        {
            elements[i]->SetPose(p);
        }
    }
}

tCompositeDrawable::DrawableElement* tCompositeDrawable::FindNode(tUID uid)
{
    for(unsigned i=0; i< elements.Size( ); i++)
    {
        if(elements[ i ] && elements[i]->GetUID() == uid)
        {
            return (elements[i]);
        }
    }
    //
    // Found nothing so return NULL
    //
    return (0);
}

int tCompositeDrawable::FindNodeIndex(tUID uid)
{
    for( unsigned i=0; i< elements.Size( ); i++)
    {
        if( elements[ i ] && elements[i]->GetUID() == uid)
        {
            return i;
        }
    }
    //
    // Found nothing so return NULL
    //
    return (-1);
}

//
// For props
//
tCompositeDrawable::DrawablePropElement* tCompositeDrawable::AddProp(tDrawable* prop, int poseIndex) 
{
    DrawablePropElement* dElement = 0;

    dElement = new DrawablePropElement;

    if(dElement)
    {
        dElement->SetVisibility(true);
        dElement->SetDrawable(prop);
        dElement->SetPoseIndex(poseIndex);
        elements.Add( dElement );
    }

    return(dElement);
}

//
// For effects
//
tCompositeDrawable::DrawableEffectElement* tCompositeDrawable::AddEffect(tEffect* effect, int poseIndex) 
{
    DrawableEffectElement* dElement = 0;

    dElement = new DrawableEffectElement;

    if(dElement)
    {
        dElement->SetDrawable(effect);
        dElement->SetVisibility(true);
        dElement->SetPoseIndex(poseIndex);
        elements.Add( dElement );
    }

    return(dElement);
}

//
// For poly skin props 
//
tCompositeDrawable::DrawablePoseElement* tCompositeDrawable::AddPose(tDrawablePose* pose) 
{
    DrawablePoseElement* dElement = 0;

    dElement = new DrawablePoseElement;

    if(dElement)
    {
        dElement->SetVisibility(true);
        dElement->SetDrawable(pose);
        elements.Add( dElement );
    }

    return(dElement);
}

void tCompositeDrawable::Remove(tUID uid)
{
    for(unsigned i=0; i< elements.Size( ); i++)
    {
        if( elements[ i ] && elements[i]->GetUID() == uid)
        {
            DrawableElement* elem = elements[i];
            elements[i] = NULL;  // No longer owned here
            delete elem;
        }
    }
}

void tCompositeDrawable::RemoveByIndex(int i)
{
    if (((unsigned)i < elements.Size( ) )&&(i>0)  && elements[ i ] )
    {
        delete elements[i];
        elements[i] = NULL;
    }
}

//-------------------------------------------------------
// Drawable Prop Element
//-------------------------------------------------------
void tCompositeDrawable::DrawablePropElement::SetDrawable(tDrawable* drawable)
{
    P3DASSERTMSG(drawable, "No drawable passed", "tCompositeDrawable::DrawablePropElement()");
    tRefCounted::Assign(prop,drawable);
}

void tCompositeDrawable::DrawablePropElement::Draw(tPose* pose)
{
#ifdef SRR2_OLD3DS_COARSE_POOLS
    Old3dsPools::Scope eligibility(Old3dsPools::Recipe,
        Old3dsPools::InspectRecipe() && RecipeProp(prop)?Old3dsPools::Upstream:Old3dsPools::Fallback);
#endif
    P3DASSERTMSG(prop, "No prop attached", "tCompositeDrawable::DrawPropElement()");

    p3d::stack->PushMultiply( (pose->GetJoint(poseIndex))->worldMatrix );
    prop->Display();
    p3d::stack->Pop();
}

//-------------------------------------------------------
// Drawable Pose Element
//-------------------------------------------------------
void tCompositeDrawable::DrawablePoseElement::SetDrawable(tDrawable* drawable)
{
    P3DASSERTMSG(drawable, "No drawable passed", "tCompositeDrawable::DrawablePoseElement()");
    
    tDrawablePose* newSkin = dynamic_cast<tDrawablePose*>(drawable);

    P3DASSERTMSG(newSkin, "Drawable not a Pose", "tCompositeDrawable::DrawablePoseElement()");

    tRefCounted::Assign(skin,newSkin);
}

void tCompositeDrawable::DrawablePoseElement::SetPose(tPose* p)
{ 
    DrawableElement::SetPose(p); 
    skin->SetPose(p); 
} 

void tCompositeDrawable::DrawablePoseElement::Draw(tPose* pose)
{
    P3DASSERTMSG(skin, "No skin attached", "tCompositeDrawable::DrawPoseElement()");

    skin->Display(pose);
}

//-------------------------------------------------------
// Drawable Effect Element
//-------------------------------------------------------
void tCompositeDrawable::DrawableEffectElement::SetDrawable(tDrawable* drawable)
{
    P3DASSERTMSG(drawable, "No drawable passed", "tCompositeDrawable::DrawableEffectElement()");
    
    tEffect* newEffect = dynamic_cast<tEffect*>(drawable);

    P3DASSERTMSG(newEffect, "Drawable not a Effect", "tCompositeDrawable::DrawableEffectElement()");

    tRefCounted::Assign(effect,newEffect);
    effect->SetVisibility(visible);
}

void tCompositeDrawable::DrawableEffectElement::Draw(tPose* pose)
{
    O3_POOL(Recipe,Fallback);
    P3DASSERTMSG(effect, "No effect attached", "tCompositeDrawable::DrawEffectElement()");

    p3d::stack->PushMultiply( (pose->GetJoint(poseIndex))->worldMatrix );
    effect->Display();
    p3d::stack->Pop();
}

//-------------------------------------------------------------------
tCompositeDrawableLoader::tCompositeDrawableLoader() : tSimpleChunkHandler(P3D_COMPOSITE_DRAWABLE)
{
}

tEntity* tCompositeDrawableLoader::LoadObject(tChunkFile* f, tEntityStore* store)
{  
    char name[128];
    f->GetPString(name);
    char skel[128];
    f->GetPString(skel);

    //
    // Have all that we need so make a new tCompositeDrawable
    //
    tCompositeDrawable* compositeDraw = new tCompositeDrawable(5);

    compositeDraw->SetName(name);
    
    //
    // Find Skeleton
    //
    tSkeleton* skeleton = p3d::find<tSkeleton>(store, skel);
    if(skeleton)
    {
        compositeDraw->skeleton = skeleton;
        compositeDraw->skeleton->AddRef();
        compositeDraw->SetPose(skeleton->NewPose());
    }

    long numTranslucentElements = 0;

    //
    // Clear Bounding Box
    //
    compositeDraw->boundingBox.high.Set(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    compositeDraw->boundingBox.low.Set( FLT_MAX,  FLT_MAX,  FLT_MAX);   

    while(f->ChunksRemaining())
    {
        f->BeginChunk();
        if(f->GetCurrentID() == P3D_COMPOSITE_DRAWABLE_SKIN_LIST)
        {
            long NumElements = f->GetLong();
            for(int i=0; i < NumElements; i++)
            {
                f->BeginChunk();
                if(f->GetCurrentID() == P3D_COMPOSITE_DRAWABLE_SKIN)
                {
                    bool isTranslucent = false; 
                    tDrawablePose* skin;
                    f->GetPString(name);
                    isTranslucent = ((f->GetLong() == 0)?false:true);
                    skin = p3d::find<tDrawablePose>(store, name);
                    if(skin)
                    {
                        tCompositeDrawable::DrawablePoseElement* compDrawSkin = compositeDraw->AddPose(skin);
                        compDrawSkin->SetIsTranslucent(isTranslucent);
                        if(isTranslucent)
                        {
                            numTranslucentElements++;
                        }

                        //
                        // Compute the overall bounding box for the CompositeDrawable
                        //
                        rmt::Box3D skinBox;
                        skin->GetBoundingBox(&skinBox);
						tPose* p = compositeDraw->GetPose();
                        skin->SetPose(p);

                        // There should probably be something about translating the skin to it's joint
                        //position, but skins in a composite drawable aren't attached to a joint. Not joint ID
                        //to read from file.
						// So here's how we'll hack it. We'll ASSUME (so automatically you know this isn't going to work
						//for you) that the skeleton is mostly for the skined object. We'll walk through the skeleton
						//joints and build a box around it. Take the center of the box and translate the skinned mesh's
						//bounding box to that location.
						rmt::Box3D skelBox;
						skelBox.high.Set(-FLT_MAX, -FLT_MAX, -FLT_MAX);
						skelBox.low.Set(FLT_MAX, FLT_MAX, FLT_MAX);
						if(!p->IsPoseReady())
						{
							p->Evaluate();
						}
						// Skip the first three joints.
						if(p)
						{
							bool haveNonOrginPoint = false;
							for(int j = 0; j < p->GetNumJoint(); ++j)
							{
								tPose::Joint* joint = p->GetJoint(j);
								if(joint)
								{
									rmt::Vector pt = joint->worldMatrix.Row(3);
									if(!haveNonOrginPoint && (pt.x == 0.0f && pt.y == 0.0f && pt.z == 0.0f))
									{
										//This is probably just a world transform or something. We can
										//skip it since the initialized skelBox will give us 0, 0, 0 point.
										continue;
									}
									haveNonOrginPoint = true;
									skelBox.Expand(joint->worldMatrix.Row(3));
								}
							}
						}
						rmt::Vector mid = skelBox.Mid();

                        compositeDraw->boundingBox.high.x = rmt::Max( (skinBox.high.x + mid.x), (compositeDraw->boundingBox.high.x) );
                        compositeDraw->boundingBox.high.y = rmt::Max( (skinBox.high.y + mid.y), (compositeDraw->boundingBox.high.y) );
                        compositeDraw->boundingBox.high.z = rmt::Max( (skinBox.high.z + mid.z), (compositeDraw->boundingBox.high.z) );

                        compositeDraw->boundingBox.low.x = rmt::Min( (skinBox.low.x + mid.x), (compositeDraw->boundingBox.low.x) );
                        compositeDraw->boundingBox.low.y = rmt::Min( (skinBox.low.y + mid.y), (compositeDraw->boundingBox.low.y) );
                        compositeDraw->boundingBox.low.z = rmt::Min( (skinBox.low.z + mid.z), (compositeDraw->boundingBox.low.z) );

                        while(f->ChunksRemaining())
                        {
                            switch(f->BeginChunk())
                            {
                                case P3D_COMPOSITE_DRAWABLE_SORTORDER:
                                    {
                                        compDrawSkin->SetSortOrder(f->GetFloat());
                                    }
                                    break;

                                default:
                                    break;
                            }
                            f->EndChunk();
                        }
                    }               
                }
                f->EndChunk();
            }
        }
        else  if(f->GetCurrentID() == P3D_COMPOSITE_DRAWABLE_EFFECT_LIST)
        {
            long NumElements = f->GetLong();
            for(int i=0; i < NumElements; i++)
            {
                f->BeginChunk();
                if(f->GetCurrentID() == P3D_COMPOSITE_DRAWABLE_EFFECT)
                {
                    bool isTranslucent = false;
                    long skeletonID = -1;
                    tEffect* effect;
                    f->GetPString(name);
                    isTranslucent = ((f->GetLong() == 0)?false:true);
                    skeletonID = f->GetLong();
                    effect = p3d::find<tEffect>(store, name);
                    if (effect)
                    {
                        tCompositeDrawable::DrawableEffectElement* compDrawEffect = compositeDraw->AddEffect(effect, skeletonID);
                        compDrawEffect->SetIsTranslucent(isTranslucent);
                        if(isTranslucent)
                        {
                            numTranslucentElements++;
                        }

                        //
                        // Compute the overall bounding box for the CompositeDrawable
                        //
                        rmt::Box3D effectBox;
                        effect->GetBoundingBox(&effectBox);
                        rmt::Matrix effectMatrix;
                        effectMatrix.Identity();
                        tSkeleton::Joint* effectJoint = skeleton->GetJoint(skeletonID);
                        effectMatrix.Mult(effectJoint->worldMatrix);

                        effectMatrix.Transform(effectBox.low, &effectBox.low);
                        effectMatrix.Transform(effectBox.high, &effectBox.high);
                        
                        // Because we can end up rotating the bounding box into who knows what angle
                        //we need to recalculate the min and the max values.
                        //
                        // X component
                        //
                        float v = rmt::Max(effectBox.low.x, effectBox.high.x);
                        compositeDraw->boundingBox.high.x = rmt::Max(v, compositeDraw->boundingBox.high.x);
                        v = rmt::Min(effectBox.low.x, effectBox.high.x);
                        compositeDraw->boundingBox.low.x = rmt::Min(v, compositeDraw->boundingBox.low.x);

                        //
                        // Y component
                        //
                        v = rmt::Max(effectBox.low.y, effectBox.high.y);
                        compositeDraw->boundingBox.high.y = rmt::Max(v, compositeDraw->boundingBox.high.y);
                        v = rmt::Min(effectBox.low.y, effectBox.high.y);
                        compositeDraw->boundingBox.low.y = rmt::Min(v, compositeDraw->boundingBox.low.y);

                        //
                        // Z component
                        //
                        v = rmt::Max(effectBox.low.z, effectBox.high.z);
                        compositeDraw->boundingBox.high.z = rmt::Max(v, compositeDraw->boundingBox.high.z);
                        v = rmt::Min(effectBox.low.z, effectBox.high.z);
                        compositeDraw->boundingBox.low.z = rmt::Min(v, compositeDraw->boundingBox.low.z);

                        while(f->ChunksRemaining())
                        {
                            switch(f->BeginChunk())
                            {
                                case P3D_COMPOSITE_DRAWABLE_SORTORDER:
                                    {
                                        compDrawEffect->SetSortOrder(f->GetFloat());
                                    }
                                    break;

                                default:
                                    break;
                            }
                            f->EndChunk();
                        }
                    }         
 
                }
                f->EndChunk();
            }
        }
        else if(f->GetCurrentID() == P3D_COMPOSITE_DRAWABLE_PROP_LIST)
        {
            long NumElements = f->GetLong();
            for(int i=0; i < NumElements; i++)
            {
                f->BeginChunk();
                if(f->GetCurrentID() == P3D_COMPOSITE_DRAWABLE_PROP)
                {
                    bool isTranslucent = false;
                    long skeletonID = -1;
                    tDrawable* prop;
                    f->GetPString(name);
                    isTranslucent = ((f->GetLong() == 0)?false:true);
                    skeletonID = f->GetLong();
                    prop = p3d::find<tDrawable>(store, name);
                    if(prop)
                    {
                        tCompositeDrawable::DrawablePropElement* compDrawProp = compositeDraw->AddProp(prop, skeletonID);
                        compDrawProp->SetIsTranslucent(isTranslucent);
                        if(isTranslucent)
                        {
                            numTranslucentElements++;
                        }

                        //
                        // Compute the overall bounding box for the CompositeDrawable
                        //
                        rmt::Box3D propBox;
                        prop->GetBoundingBox(&propBox);
                        rmt::Matrix propMatrix;
                        propMatrix.Identity();
                        tSkeleton::Joint* propJoint = skeleton->GetJoint(skeletonID);
                        propMatrix.Mult(propJoint->worldMatrix);

                        propMatrix.Transform(propBox.low, &propBox.low);
                        propMatrix.Transform(propBox.high, &propBox.high);

                        // Because we can end up rotating the bounding box into who knows what angle
                        //we need to recalculate the min and the max values.
                        //
                        // X component
                        //
                        float v = rmt::Max(propBox.low.x, propBox.high.x);
                        compositeDraw->boundingBox.high.x = rmt::Max(v, compositeDraw->boundingBox.high.x);
                        v = rmt::Min(propBox.low.x, propBox.high.x);
                        compositeDraw->boundingBox.low.x = rmt::Min(v, compositeDraw->boundingBox.low.x);

                        //
                        // Y component
                        //
                        v = rmt::Max(propBox.low.y, propBox.high.y);
                        compositeDraw->boundingBox.high.y = rmt::Max(v, compositeDraw->boundingBox.high.y);
                        v = rmt::Min(propBox.low.y, propBox.high.y);
                        compositeDraw->boundingBox.low.y = rmt::Min(v, compositeDraw->boundingBox.low.y);

                        //
                        // Z component
                        //
                        v = rmt::Max(propBox.low.z, propBox.high.z);
                        compositeDraw->boundingBox.high.z = rmt::Max(v, compositeDraw->boundingBox.high.z);
                        v = rmt::Min(propBox.low.z, propBox.high.z);
                        compositeDraw->boundingBox.low.z = rmt::Min(v, compositeDraw->boundingBox.low.z);

                        while(f->ChunksRemaining())
                        {
                            switch(f->BeginChunk())
                            {
                                case P3D_COMPOSITE_DRAWABLE_SORTORDER:
                                    {
                                        compDrawProp->SetSortOrder(f->GetFloat());
                                    }
                                    break;

                                default:
                                    break;
                            }
                            f->EndChunk();
                        }
                    }


                }
                f->EndChunk();
            }
        }
        f->EndChunk();
    }

    // really hacky sphere 
    compositeDraw->boundingSphere.centre.x = (compositeDraw->boundingBox.high.x + compositeDraw->boundingBox.low.x) / 2;
    compositeDraw->boundingSphere.centre.y = (compositeDraw->boundingBox.high.y + compositeDraw->boundingBox.low.y) / 2;
    compositeDraw->boundingSphere.centre.z = (compositeDraw->boundingBox.high.z + compositeDraw->boundingBox.low.z) / 2;
    rmt::Vector v;
    v.Sub(compositeDraw->boundingBox.high, compositeDraw->boundingSphere.centre);
    compositeDraw->boundingSphere.radius = v.Magnitude();

    compositeDraw->translucentObjects.SetSize(numTranslucentElements);

    return(compositeDraw);
}


