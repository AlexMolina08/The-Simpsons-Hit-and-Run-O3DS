//========================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        AllWrappers.cpp
//
// Description: Implementation for AllWrappers class.
//
// History:     Implemented	                         --Devin [4/24/2002]
//========================================================================

//========================================
// System Includes
//========================================

//========================================
// Project Includes
//========================================
#include <raddebug.hpp>
#include <memory/srrmemory.h>

#include <render/Loaders/AllWrappers.h>
#include <render/Loaders/GeometryWrappedLoader.h>
#include <render/Loaders/StaticEntityLoader.h>
#include <render/Loaders/StaticPhysLoader.h>
#include <render/Loaders/TreeDSGLoader.h>
#include <render/Loaders/FenceLoader.h>
#include <render/Loaders/IntersectLoader.h>
#include <render/Loaders/AnimCollLoader.h>
#include <render/Loaders/AnimDSGLoader.h>
#include <render/Loaders/DynaPhysLoader.h>
#include <render/Loaders/InstStatPhysLoader.h>
#include <render/Loaders/InstStatEntityLoader.h>
#include <render/Loaders/WorldSphereLoader.h>
#include <render/Loaders/AnimDynaPhysLoader.h>
#include <loading/roadloader.h>
#include <loading/pathloader.h>
#include <render/Loaders/BillboardWrappedLoader.h>
#include <render/Loaders/instparticlesystemloader.h>
#include <render/Loaders/breakableobjectloader.h>
#include <render/Loaders/LensFlareLoader.h>
#include <loading/locatorloader.h>
#include <p3d/drawable.hpp>

//************************************************************************
//
// Global Data, Local Data, Local Classes
//
//************************************************************************

AllWrappers* AllWrappers::mspInstance = NULL;

static const int NUM_GLOBAL_ENTITIES = 32;
#ifdef SRR2_OLD3DS_NATIVE_BENCH
#include <cstdio>
#include <cstring>
namespace {
AllWrappers* expectedWrappers=nullptr;
IWrappedLoader* expectedLoaders[AllWrappers::msNumWrappers]={};
void* expectedVtables[AllWrappers::msNumWrappers]={};
GeometryWrappedLoader* expectedGeometry=nullptr;
void* expectedGeometryVtable=nullptr;
unsigned loaderIntegrityFailures=0;
}
#endif


//************************************************************************
//
// Public Member Functions : AllWrappers Interface
//
//************************************************************************

//========================================================================
// AllWrappers::CreateInstance
//========================================================================
//
// Description: 
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
AllWrappers* AllWrappers::CreateInstance()
{
MEMTRACK_PUSH_GROUP( "AllWrappers" );
   rAssert(mspInstance == NULL);

   mspInstance = new(GMA_PERSISTENT) AllWrappers;
MEMTRACK_POP_GROUP( "AllWrappers" );

   return mspInstance;
}

//========================================================================
// AllWrappers::GetInstance
//========================================================================
//
// Description: 
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
AllWrappers* AllWrappers::GetInstance()
{
   rAssert(mspInstance != NULL);

   return mspInstance;
}

//========================================================================
// AllWrappers::DestroyInstance
//========================================================================
//
// Description: 
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void  AllWrappers::DestroyInstance()
{
   rAssert(mspInstance != NULL);

   delete mspInstance;
}

//========================================================================
// AllWrappers::mLoader( int iIndex )
//========================================================================
//
// Description: Get iIndexth's element
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
IWrappedLoader& AllWrappers::mLoader( int iIndex )
{
   rAssert( iIndex < msNumWrappers);

   return *(mpLoaders[iIndex]);
}

//========================================================================
// AllWrappers::mpLoader( int iIndex )
//========================================================================
//
// Description: Get iIndexth's element's ptr
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
IWrappedLoader* AllWrappers::mpLoader( int iIndex )
{
   rAssert( iIndex < msNumWrappers);

#ifdef SRR2_OLD3DS_NATIVE_BENCH
   if(iIndex==msGeometry && expectedWrappers){
       void* vtable=nullptr;std::memcpy(&vtable,expectedGeometry,sizeof(vtable));
       if(this!=expectedWrappers || mpLoaders[iIndex]!=expectedLoaders[iIndex] || vtable!=expectedGeometryVtable)
           DumpIntegrity(stdout);
   }
#endif
   return (mpLoaders[iIndex]);
}

//************************************************************************
//
// Protected Member Functions : AllWrappers 
//
//************************************************************************
void AllWrappers::CoupleAllLoaders()
{
MEMTRACK_PUSH_GROUP( "AllWrappers" );
   IWrappedLoader* pWL = NULL;

   for( int i=msNumWrappers-1; i>-1; i-- )
   {
      rAssert( mpLoaders[i] == NULL );

      switch( i ) 
      {
      case msGeometry:
         {
            pWL = new(GMA_PERSISTENT) GeometryWrappedLoader();
            ((GeometryWrappedLoader*)pWL)->AddRef();
            mpLoaders[i] = pWL;
         }
      	break;
         
      case msStaticPhys:
         {
            
            pWL = new(GMA_PERSISTENT) StaticPhysLoader();
            ((StaticPhysLoader*)pWL)->AddRef();
            mpLoaders[i] = pWL;
         }
      	break;

      case msStaticEntity:
         {
            pWL = new(GMA_PERSISTENT) StaticEntityLoader();
            ((StaticEntityLoader*)pWL)->AddRef();
            mpLoaders[i] = pWL;
         }
      	break;
      
      case msTreeDSG:
         {
            pWL = new(GMA_PERSISTENT) TreeDSGLoader();
            ((TreeDSGLoader*)pWL)->AddRef();
            mpLoaders[i] = pWL;
         }
      	break;
         
      case msFenceEntity:
         {
            pWL = new(GMA_PERSISTENT) FenceLoader();
            ((FenceLoader*)pWL)->AddRef();
            mpLoaders[i] = pWL;
         }
         break;
         
      case msIntersectDSG:
         {
            pWL = new(GMA_PERSISTENT) IntersectLoader();
            ((IntersectLoader*)pWL)->AddRef();
            mpLoaders[i] = pWL;
         }
         break;

      case msAnimCollEntity:
         {
            pWL = new(GMA_PERSISTENT) AnimCollLoader();
            ((AnimCollLoader*)pWL)->AddRef();
            mpLoaders[i] = pWL;
         }
         break;

      case msDynaPhys:
         {
             pWL = new(GMA_PERSISTENT) DynaPhysLoader();
             ((DynaPhysLoader*)pWL)->AddRef();
             mpLoaders[i] =  pWL;
         }
         break;

      case msInstStatEntity:
          {
             pWL = new(GMA_PERSISTENT) InstStatEntityLoader();
             ((InstStatEntityLoader*)pWL)->AddRef();
             mpLoaders[i] =  pWL;
          }
          break;

      case msInstStatPhys:
          {
             pWL = new(GMA_PERSISTENT) InstStatPhysLoader();
             ((InstStatPhysLoader*)pWL)->AddRef();
             mpLoaders[i] =  pWL;
          }
          break;
 
      case msLocator:
          {
             pWL = new(GMA_PERSISTENT) LocatorLoader();
             ((LocatorLoader*)pWL)->AddRef();
             mpLoaders[i] =  pWL;
          }
          break;

      case msWorldSphere:
          {
             pWL = new(GMA_PERSISTENT) WorldSphereLoader();
             ((WorldSphereLoader*)pWL)->AddRef();
             mpLoaders[i] =  pWL;
          }
          break;

      case msRoadSegment:
          {
             pWL = new(GMA_PERSISTENT) RoadLoader();
             ((RoadLoader*)pWL)->AddRef();
             mpLoaders[i] =  pWL;
          }
          break;

      case msPathSegment:
          {
             pWL = new(GMA_PERSISTENT) PathLoader();
             ((PathLoader*)pWL)->AddRef();
             mpLoaders[i] =  pWL;
          }
          break;

      case msBillboard:
          {
             pWL = new(GMA_PERSISTENT) BillboardWrappedLoader();
             ((BillboardWrappedLoader*)pWL)->AddRef();
             mpLoaders[i] =  pWL;
          }
          break;
      case msInstParticleSystem:
          {
             pWL = new (GMA_PERSISTENT) InstParticleSystemLoader();
             ((InstParticleSystemLoader*)pWL)->AddRef();
             mpLoaders[i] = pWL;
          }
          break;
      case msBreakableObject:
          {
             pWL = new (GMA_PERSISTENT) BreakableObjectLoader();
             ((BreakableObjectLoader*)pWL)->AddRef();
             mpLoaders[i] = pWL;
          }
          break;
      case msAnimEntity:
          {
             pWL = new (GMA_PERSISTENT) AnimDSGLoader();
             ((AnimDSGLoader*)pWL)->AddRef();
             mpLoaders[i] = pWL;
          }
          break;
	  case msLensFlare:
		  {
			pWL = new(GMA_PERSISTENT) LensFlareLoader();
			((LensFlareLoader*)pWL)->AddRef();
			mpLoaders[i] = pWL;
		  }
		  break;
	  case msAnimDynaPhys:
		  {
			pWL = new(GMA_PERSISTENT) AnimDynaPhysLoader();
			((AnimDynaPhysLoader*)pWL)->AddRef();
			mpLoaders[i] = pWL;
		  }
		  break;
	  case msAnimDynaPhysWrapper:
		  {
			pWL = new (GMA_PERSISTENT) AnimDynaPhysWrapperLoader();
			((AnimDynaPhysWrapperLoader*)pWL)->AddRef();
			mpLoaders[i] = pWL;			
		  }
		  break;
      default:
         rAssert(false);
         break;
      }
   }
MEMTRACK_POP_GROUP( "AllWrappers" );
}

void AllWrappers::AddGlobalEntity(tDrawable* Entity)
{
    if( !Entity )
    {
        return;
    }
#ifdef SRR2_OLD3DS_NATIVE_BENCH
    if(mNumGlobalEntities>=NUM_GLOBAL_ENTITIES)
        std::printf("NATIVE_GLOBAL_ENTITY_OVERFLOW count=%d capacity=%d\n",mNumGlobalEntities,NUM_GLOBAL_ENTITIES);
#endif
    rAssert(mNumGlobalEntities < NUM_GLOBAL_ENTITIES);
    rAssert(GetGlobalEntity(Entity->GetUID()) == 0);
    mpGlobalEntities[mNumGlobalEntities] = Entity;
    mpGlobalEntities[mNumGlobalEntities]->AddRef();
    ++mNumGlobalEntities;
}

tDrawable* AllWrappers::GetGlobalEntity(tUID EntityID) const
{
    for(int i = 0; i < mNumGlobalEntities; ++i)
    {
        if(mpGlobalEntities[i]->GetUID() == EntityID )
        {
            return mpGlobalEntities[i];
        }
    }
    return 0;
}

void AllWrappers::ClearGlobalEntities(void)
{
    for(int i = 0; i < mNumGlobalEntities; ++i)
    {
        tRefCounted::Release(mpGlobalEntities[i]);
    }
    mNumGlobalEntities = 0;
}


//************************************************************************
//
// Private Member Functions : AllWrappers 
//
//************************************************************************

//========================================================================
// AllWrappers::AllWrappers
//========================================================================
//
// Description: 
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
AllWrappers::AllWrappers()
{
   for( int i=msNumWrappers-1; i>-1; i--)
   {
      mpLoaders[i] = NULL;
   }
   CoupleAllLoaders();

#ifdef SRR2_OLD3DS_NATIVE_BENCH
   expectedWrappers=this;
   for(int i=0;i<msNumWrappers;++i){expectedLoaders[i]=mpLoaders[i];std::memcpy(expectedVtables+i,mpLoaders[i],sizeof(void*));}
   expectedGeometry=static_cast<GeometryWrappedLoader*>(mpLoaders[msGeometry]);
   std::memcpy(&expectedGeometryVtable,expectedGeometry,sizeof(void*));
#endif
   mpGlobalEntities = new tDrawable* [NUM_GLOBAL_ENTITIES];
   rAssert(mpGlobalEntities);
   mNumGlobalEntities = 0;
}
   
//========================================================================
// AllWrappers::~AllWrappers()
//========================================================================
//
// Description: 
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
AllWrappers::~AllWrappers()
{
   // this codeblock does not apply anymore because the inheritance tree
   // was changed.. leaving it in just in case.

   //for( int i=msNumWrappers-1; i>-1; i--)
   //{
   //   if( mpLoaders[i] != NULL )
   //   {

   //      ( static_cast<tRefCounted*>(mpLoaders[i]) )->Release();
   //      mpLoaders[i] = NULL;
   //   }
   //}
    ClearGlobalEntities();
    delete mpGlobalEntities;
    mpGlobalEntities = 0;
}

#ifdef SRR2_OLD3DS_NATIVE_BENCH
void AllWrappers::DumpIntegrity(FILE* file) const
{
    unsigned changed=0,vtables=0;
    if(this!=expectedWrappers)changed=~0u;
    else for(unsigned i=0;i<msNumWrappers;++i){
        if(mpLoaders[i]!=expectedLoaders[i])changed|=1u<<i;
        else{void* vtable=nullptr;std::memcpy(&vtable,mpLoaders[i],sizeof(vtable));if(vtable!=expectedVtables[i])vtables|=1u<<i;}
    }
    void* geometryVtable=nullptr;
    if(expectedGeometry)std::memcpy(&geometryVtable,expectedGeometry,sizeof(geometryVtable));
    const bool geometryBad=geometryVtable!=expectedGeometryVtable;
    loaderIntegrityFailures+=changed || vtables || geometryBad;
    std::fprintf(file,"NATIVE_WRAPPER_INTEGRITY instance=%p expected_instance=%p geometry=%p expected_geometry=%p changed_slots=%08x changed_vtables=%08x geometry_vtable=%p expected_vtable=%p failures=%u global_entities=%d capacity=%d\n",
        static_cast<const void*>(this),static_cast<void*>(expectedWrappers),
        this==expectedWrappers?static_cast<void*>(mpLoaders[msGeometry]):nullptr,static_cast<void*>(expectedLoaders[msGeometry]),
        changed,vtables,geometryVtable,expectedGeometryVtable,loaderIntegrityFailures,mNumGlobalEntities,NUM_GLOBAL_ENTITIES);
}
#endif
