//========================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        StaticEntityLoader.cpp
//
// Description: Implementation for StaticEntityLoader class.
//
// History:     Implemented	                         --Devin [5/27/2002]
//========================================================================

//========================================
// System Includes
//========================================
#if defined(SRR2_OLD3DS_NATIVE_BENCH) && defined(SRR2_OLD3DS_VISUAL_UPGRADE)
#include <main/nativebench.h>
#include <cstdio>
#include <cstring>
#endif
#include <p3d/chunkfile.hpp>
#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
#include <main/landmarkvisibility.h>
#include <cstring>
#endif
#include <p3d/utility.hpp>

//========================================
// Project Includes
//========================================
#include <render/Loaders/StaticEntityLoader.h>
#include <render/Loaders/GeometryWrappedLoader.h>
#include <render/DSG/StaticEntityDSG.h>

#include <constants/srrchunks.h>
#include <constants/chunkids.hpp>
#include <memory/srrmemory.h>

#include <render/Loaders/AllWrappers.h>

//************************************************************************
//
// Global Data, Local Data, Local Classes
//
//************************************************************************

//************************************************************************
//
// Public Member Functions : StaticEntityLoader Interface
//
//************************************************************************
//========================================================================
// StaticEntityLoader::
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
StaticEntityLoader::StaticEntityLoader() :
tSimpleChunkHandler(SRR2::ChunkID::ENTITY_DSG)
{
   mpListenerCB  = NULL;
   mUserData     = -1;
}
///////////////////////////////////////////////////////////////////////
// tSimpleChunkHandler
///////////////////////////////////////////////////////////////////////
//========================================================================
// StaticEntityLoader::
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
tEntity* StaticEntityLoader::LoadObject(tChunkFile* f, tEntityStore* store)
{
    IEntityDSG::msDeletionsSafe=true;
    char name[255];
    f->GetPString(name);

    int version = f->GetLong();
    int HasAlpha = f->GetLong();

    StaticEntityDSG *pStaticEntityDSG = new StaticEntityDSG;
    pStaticEntityDSG->SetName(name);
#if defined(SRR2_OLD3DS_NATIVE_BENCH) && defined(SRR2_OLD3DS_VISUAL_UPGRADE)
    if(NativeBench::LandmarkDiagnostic() &&
       (!strncmp(name,"r2_gens39Shape",13) || !strcmp(name,"mrburns") || !strcmp(name,"casino_lights")))
        printf("NATIVE_LANDMARK_LOAD name=%s loader=pStaticEntityDSG loading_only=1\n",name);
#endif


    if(HasAlpha)
    {
        pStaticEntityDSG->mTranslucent = true;
    }
    
    while(f->ChunksRemaining())
    {      
        f->BeginChunk();
        switch(f->GetCurrentID())
        {
            case Pure3D::Mesh::MESH:
            {
                GeometryWrappedLoader* pGeoLoader = (GeometryWrappedLoader*)AllWrappers::GetInstance()->mpLoader(AllWrappers::msGeometry) ;
                rAssert(pGeoLoader!=NULL);
				tGeometry* pGeo = (tGeometry*)pGeoLoader->LoadObject(f, store);
                rAssert(pGeo!=NULL);
                pStaticEntityDSG->SetGeometry(pGeo);
            }

            default:
                break;
        } // switch
        f->EndChunk();
    } // while

#ifdef SRR2_OLD3DS_VISUAL_UPGRADE
    if(Old3dsLandmark::enabled && !HasAlpha)
    {
        rmt::Sphere sphere;
        rmt::Box3D box;
        pStaticEntityDSG->GetBoundingSphere(&sphere);
        pStaticEntityDSG->GetBoundingBox(&box);
        const float bounds[]={sphere.centre.x,sphere.centre.y,sphere.centre.z,sphere.radius,
            box.low.x,box.low.y,box.low.z,box.high.x,box.high.y,box.high.z};
        pStaticEntityDSG->mOld3dsLandmark=Old3dsLandmark::SelectStatic(f->GetFilename(),name,bounds);
#ifdef SRR2_OLD3DS_NATIVE_BENCH
        if(pStaticEntityDSG->mOld3dsLandmark) ++Old3dsLandmark::selected;
#endif
    }
#endif

    mpListenerCB->OnChunkLoaded( pStaticEntityDSG, mUserData, _id );
    IEntityDSG::msDeletionsSafe=false;
    return pStaticEntityDSG;
}
///////////////////////////////////////////////////////////////////////
// IWrappedLoader
///////////////////////////////////////////////////////////////////////
//========================================================================
// StaticEntityLoader::SetRegdListener
//========================================================================
//
// Description: Register a new listener/caretaker, notify old listener of 
//              severed connection.
//
// Parameters:  pListenerCB: Callback to call OnChunkLoaded
//              pUserData:   Data to pass along for filtering, etc
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void StaticEntityLoader::SetRegdListener
(
   ChunkListenerCallback* pListenerCB,
   int iUserData 
)
{
   //
   // Follow protocol; notify old Listener, that it has been 
   // "disconnected".
   //
   if( mpListenerCB != NULL )
   {
      mpListenerCB->OnChunkLoaded( NULL, iUserData, 0 );
   }
   mpListenerCB  = pListenerCB;
   mUserData     = iUserData;
}

//========================================================================
// StaticEntityLoader::ModRegdListener
//========================================================================
//
// Description: Just fuck with the current pUserData
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void StaticEntityLoader::ModRegdListener
( 
   ChunkListenerCallback* pListenerCB,
   int iUserData 
)
{
#if 0
   char DebugBuf[255];
   sprintf( DebugBuf, "GeometryWrappedLoader::ModRegdListener: pListenerCB %X vs mpListenerCB %X\n", pListenerCB, mpListenerCB );
   rDebugString( DebugBuf );
#endif
   rAssert( pListenerCB == mpListenerCB );

   mUserData = iUserData;
}
//************************************************************************
//
// Protected Member Functions : StaticEntityLoader 
//
//************************************************************************

//************************************************************************
//
// Private Member Functions : StaticEntityLoader 
//
//************************************************************************
