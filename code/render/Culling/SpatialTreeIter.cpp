//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        SpatialTreeIter.cpp
//
// Description: Implement SpatialTreeIter
//
// History:     16/07/2002 + Created -- Darren
//
//=============================================================================

//========================================
// System Includes
//========================================

//========================================
// Project Includes
//========================================
#include <render/Culling/SpatialTreeIter.h>


//******************************************************************************
//
// Global Data, Local Data, Local Classes
//
//******************************************************************************

//******************************************************************************
//
// Public Member Functions
//
//******************************************************************************

//========================================================================
// SpatialTreeIter::
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
 
SpatialNode&   SpatialTreeIter::rSeekNode
(
   ISpatialProxyAA& irVolume, 
   int              iCurNodeOffset 
)
{
   ContiguousBinNode< SpatialNode >* pCurNode = (mpRootNode+iCurNodeOffset);

   if( pCurNode->GetSubTreeSize() > 0 )
   {
     //  if(pCurNode->mData.mSubDivPlane.mAxis ==2 && pCurNode->mData.mSubDivPlane.mPosn ==0.0f)
     //      rReleasePrintf("ackaMaAck\n");
      float PlaneVolResult = irVolume.CompareTo( pCurNode->mData.mSubDivPlane );

      if( PlaneVolResult > 0.0f ) //The Plane is greater than TVolume in Posn, so TVolume is in the LT Partitiion..
      {
         return rSeekNode( irVolume, iCurNodeOffset + pCurNode->LChildOffset() );
      } 
      else
      {
         if( PlaneVolResult < 0.0f ) //The Plane is less han TVolume in Posn, so TVolume is in the GT Partitiion..
         {
            return rSeekNode( irVolume, iCurNodeOffset + pCurNode->RChildOffset() );
         }
         else //  PlaneVolResult == 0 
         {
            return (pCurNode->mData);
         }
      }
   }
   else
   {
      return (pCurNode->mData);
   }
}

//========================================================================
// SpatialTreeIter::
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
  
void SpatialTreeIter::SeekSubNodes
( 
   ISpatialProxyAA& irVolume,
   UseArray<int>&   orNodeOffsets,
   int              iCurNodeOffset 
)
{
   ContiguousBinNode< SpatialNode >* pCurNode = (mpRootNode+iCurNodeOffset);

   if( pCurNode->GetSubTreeSize() > 0 )
   {
      float PlaneVolResult = irVolume.CompareTo( pCurNode->mData.mSubDivPlane );

      if( PlaneVolResult > 0.0f ) //The Plane is greater than TVolume in Posn, so TVolume is in the LT Partitiion..
      {
         SeekSubNodes( irVolume, orNodeOffsets, iCurNodeOffset + pCurNode->LChildOffset() );
      } 
      else
      {
         if( PlaneVolResult < 0.0f ) //The Plane is less han TVolume in Posn, so TVolume is in the GT Partitiion..
         {
            SeekSubNodes( irVolume, orNodeOffsets, iCurNodeOffset + pCurNode->RChildOffset() );
         }
         else //  PlaneVolResult == 0 
         {
            //return *pCurNode;
            int max = pCurNode->GetSubTreeSize()+1;
            for(int i=0; i<max; i++, iCurNodeOffset++)
            {
               orNodeOffsets.Add(iCurNodeOffset);
            }
            
         }
      }
   }
   else
   {
      //return *pCurNode;
      orNodeOffsets.Add(iCurNodeOffset);
   }
}

//========================================================================
// SpatialTreeIter::
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
  
void SpatialTreeIter::SeekAllNodes
( 
   ISpatialProxyAA& irVolume,
   UseArray<int>&   orNodeOffsets,
   int              iCurNodeOffset 
)
{
   ContiguousBinNode< SpatialNode >* pCurNode = (mpRootNode+iCurNodeOffset);

   if( pCurNode->GetSubTreeSize() > 0 )
   {
      float PlaneVolResult = irVolume.CompareTo( pCurNode->mData.mSubDivPlane );

      if( PlaneVolResult > 0.0f ) //The Plane is greater than TVolume in Posn, so TVolume is in the LT Partitiion..
      {
         orNodeOffsets.Add(iCurNodeOffset);
         SeekAllNodes( irVolume, orNodeOffsets, iCurNodeOffset + pCurNode->LChildOffset() );
      } 
      else
      {
         if( PlaneVolResult < 0.0f ) //The Plane is less han TVolume in Posn, so TVolume is in the GT Partitiion..
         {
            orNodeOffsets.Add(iCurNodeOffset);
            SeekAllNodes( irVolume, orNodeOffsets, iCurNodeOffset + pCurNode->RChildOffset() );
         }
         else //  PlaneVolResult == 0 
         {
            //return *pCurNode;
            int max = pCurNode->GetSubTreeSize()+1;
            for(int i=0; i<max; i++, iCurNodeOffset++)
            {
               orNodeOffsets.Add(iCurNodeOffset);
            }
            
         }
      }
   }
   else
   {
      //return *pCurNode;
      orNodeOffsets.Add(iCurNodeOffset);
   }
}

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////  
SpatialNode& SpatialTreeIter::rSeekLeaf
( 
   int iCurNodeOffset, 
   Vector3f& irPt 
)
{
   ContiguousBinNode< SpatialNode >* pCurNode = (mpRootNode+iCurNodeOffset);

   if( pCurNode->GetSubTreeSize() > 0 )
   {
      if( irPt[(int)pCurNode->mData.mSubDivPlane.mAxis] < pCurNode->mData.mSubDivPlane.mPosn  )
      {
         return rSeekLeaf( iCurNodeOffset + pCurNode->LChildOffset(), irPt );
      }
      else
      {
         return rSeekLeaf( iCurNodeOffset + pCurNode->RChildOffset(), irPt );
      }
   }
   else
   {
      return pCurNode->mData;
   }
}

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////  
void SpatialTreeIter::MarkAll( int                 iCurNodeOffset, 
                                  BoxPts&             irBoxPts, 
                                  SphereSP&    irDesiredVolume, 
                                  tMark               iMark )
{
#ifdef SRR2_OLD3DS_SPATIAL_WALK_FAST
   MarkSpherePlanes(iCurNodeOffset, irDesiredVolume, iMark);
#else
   ContiguousBinNode< SpatialNode >* pCurNode = (mpRootNode+iCurNodeOffset);

   SpatialNode* pFuckinCast;
   pFuckinCast = &(pCurNode->mData);
   mCurNodes.Add( pFuckinCast );
   mNodeMarks[iCurNodeOffset] |= iMark;

   if( pCurNode->GetSubTreeSize() == 0 ) //if leaf, marked, so leave
   {
      return;
   }

   float intersectResult = irDesiredVolume.CompareTo(pCurNode->mData.mSubDivPlane);
   if( intersectResult == 0.0f ) //intersection
   {
        //split subspace representation, and recurse
        BoxPts BoxPtsLT;
        BoxPtsLT = irBoxPts;

        BoxPtsLT.CutOffGT( pCurNode->mData.mSubDivPlane );

        MarkAll( iCurNodeOffset + pCurNode->LChildOffset(), BoxPtsLT, irDesiredVolume, iMark );   

        irBoxPts.CutOffLT( pCurNode->mData.mSubDivPlane );

        MarkAll( iCurNodeOffset + pCurNode->RChildOffset(), irBoxPts, irDesiredVolume, iMark );
   }
   else
   {
       if(intersectResult>0.0f) //Plane falls on the right, so recurse lt side
       {
            irBoxPts.CutOffGT( pCurNode->mData.mSubDivPlane );

            MarkAll( iCurNodeOffset + pCurNode->LChildOffset(), irBoxPts, irDesiredVolume, iMark );
       }
       else //Plane falls on the left(lt), so recurse gt side
       {
            irBoxPts.CutOffLT( pCurNode->mData.mSubDivPlane );

            MarkAll( iCurNodeOffset + pCurNode->RChildOffset(), irBoxPts, irDesiredVolume, iMark );
       }
   }
#endif
#if 0
   if( irBoxPts.TestNotOutside( irDesiredVolume ) < 0.0f ) 
   {
      //if any pt, or the volume defined by said pts of desired volume is in the AABB...
      mNodeMarks[iCurNodeOffset] |= iMark;

      //If all of the pts of the AABB are inside the Desired Volume, 
      //just mark the whole subtree, otherwise recurse

      //BEGIN_PROFILE("Tree Compare")

      int nPtsInside = 0;
      //  Ignore whole subtree marks until it pays off query volume vs subtree 
      // size... put test here to this effect
      for( int i=0; i<8 && !nPtsInside; i++ )//  
      {
         if( irDesiredVolume.CompareTo( irBoxPts[i] ) > 0.0f ) 
         {
             break;
         }
            //Pt is inside SpatialProxy
            nPtsInside++;
      }

      //END_PROFILE("Tree Compare")

      //debug: make this inclusion threshold a parameter
      //if( nPtsInside > 5 && nPtsInside !=8 )
      //   nPtsInside = 8;

      SpatialNode* pFuckinCast;
      pFuckinCast = &(pCurNode->mData);
      mCurNodes.Add( pFuckinCast );
      // if the entire subspace is within the desired volume
      switch( nPtsInside )
      {
      case 8:
         //BEGIN_PROFILE("SubTree Marked")
         mNodeMarks[iCurNodeOffset] |= iMark;
         MarkSubTree( iCurNodeOffset, iMark );
         //END_PROFILE("SubTree Marked")
         break;
       
      default: 
         mNodeMarks[iCurNodeOffset] |= iMark;
         //partial intersection; recurse 
         if( pCurNode->GetSubTreeSize() > 0 )
         {
            //split subspace representation, and recurse
            BoxPts BoxPtsLT;
            BoxPtsLT = irBoxPts;

            BoxPtsLT.CutOffGT( pCurNode->mData.mSubDivPlane );

            MarkAll( iCurNodeOffset + pCurNode->LChildOffset(), BoxPtsLT, irDesiredVolume, iMark );   
      
            irBoxPts.CutOffLT( pCurNode->mData.mSubDivPlane );
      
            MarkAll( iCurNodeOffset + pCurNode->RChildOffset(), irBoxPts, irDesiredVolume, iMark );
         }
         break;

      }
   } //else, no desired volume and AABB don't intersect, therefore do nothing
#endif
   /*
   int nPtsInside = 0;
   for( int i=0; i<8; i++ )
   {
      if( irDesiredVolume.CompareTo( irBoxPts[i] ) < 0.0f ) 
      {
         //Pt is inside SpatialProxy
         nPtsInside++;
      }
   }

   ContiguousBinNode< SpatialNode >* pCurNode = (mpRootNode+iCurNodeOffset);

   // if the entire subspace is within the desired volume
   switch( nPtsInside )
   {
   case 8:
      mNodeMarks[iCurNodeOffset] = iMark;
      MarkSubTree( iCurNodeOffset, iMark );
      break;

   case 0: 
      //Find out whether the subtree is outside the volume
      //or the subtree envelopes the volume
//      if( irBoxPts.CompareTo( irDesiredVolume.GetPoint() ) < 0.0f )
//      {
//         mNodeMarks[iCurNodeOffset] = iMark;
//         MarkSubTree( iCurNodeOffset, iMark );
//      }
      //TODO: can reuse the code in default by saying if compare fails break (else continue, no break;).

      if( irBoxPts.CompareTo( irDesiredVolume.GetPoint() ) < 0.0f )
      //if( irBoxPts.TestNotOutside( irDesiredVolume ) < 0.0f ) //if any pt, or the volume defined by said pts of desired volume is in the AABB...
      {
         mNodeMarks[iCurNodeOffset] = iMark;

         if( pCurNode->GetSubTreeSize() > 0 )
         {
            //split subspace representation, and recurse
            BoxPts BoxPtsLT;
            BoxPtsLT = irBoxPts;

            BoxPtsLT.CutOffGT( pCurNode->mData.mSubDivPlane );

            MarkAll( iCurNodeOffset + pCurNode->LChildOffset(), BoxPtsLT, irDesiredVolume, iMark );   
      
            irBoxPts.CutOffLT( pCurNode->mData.mSubDivPlane );
      
            MarkAll( iCurNodeOffset + pCurNode->RChildOffset(), irBoxPts, irDesiredVolume, iMark );
         }
      }
      break;


   default: 
      mNodeMarks[iCurNodeOffset] = iMark;
      //partial intersection; recurse 
      if( pCurNode->GetSubTreeSize() > 0 )
      {
         //split subspace representation, and recurse
         BoxPts BoxPtsLT;
         BoxPtsLT = irBoxPts;

         BoxPtsLT.CutOffGT( pCurNode->mData.mSubDivPlane );

         MarkAll( iCurNodeOffset + pCurNode->LChildOffset(), BoxPtsLT, irDesiredVolume, iMark );   
      
         irBoxPts.CutOffLT( pCurNode->mData.mSubDivPlane );
      
         MarkAll( iCurNodeOffset + pCurNode->RChildOffset(), irBoxPts, irDesiredVolume, iMark );
      }
      break;
   }
   */
}
void SpatialTreeIter::MarkAllSphere
(  
    int         iCurNodeOffset,
    BoxPts&     irBoxPts, 
    SphereSP&   irSphere, 
    tMark       iMark 
)
{
   if(      (iMark != 0x0f)
       &&   (mNodeMarks[iCurNodeOffset] & msFilterInvisible) )
   {
       return;
   }
   if(      (iMark != 0x0f)
       &&   (mNodeMarks[iCurNodeOffset] & msFilterVisible) )
   {
       return; 
       //because this chunk will always be visible, and tracked during the marksubtrees
   }
   ContiguousBinNode< SpatialNode >* pCurNode = (mpRootNode+iCurNodeOffset);

   SpatialNode* pFuckinCast;
   pFuckinCast = &(pCurNode->mData);
   mCurNodes.Add( pFuckinCast );
   mNodeMarks[iCurNodeOffset] |= iMark;

   if( pCurNode->GetSubTreeSize() == 0 ) //if leaf, marked, so leave
   {
      return;
   }


   float intersectResult = irSphere.CompareTo(pCurNode->mData.mSubDivPlane);
   if( intersectResult == 0.0f ) //intersection
   {
        //split subspace representation, and recurse
        BoxPts BoxPtsLT;
        BoxPtsLT = irBoxPts;

        BoxPtsLT.CutOffGT( pCurNode->mData.mSubDivPlane );

        MarkAllSphereIntersect( iCurNodeOffset + pCurNode->LChildOffset(), BoxPtsLT, irSphere, iMark );   

        irBoxPts.CutOffLT( pCurNode->mData.mSubDivPlane );

        MarkAllSphereIntersect( iCurNodeOffset + pCurNode->RChildOffset(), irBoxPts, irSphere, iMark );
   }
   else
   {
       if(intersectResult>0.0f) //Plane falls on the right, so recurse lt side
       {
            irBoxPts.CutOffGT( pCurNode->mData.mSubDivPlane );

            MarkAllSphere( iCurNodeOffset + pCurNode->LChildOffset(), irBoxPts, irSphere, iMark );
       }
       else //Plane falls on the left(lt), so recurse gt side
       {
            irBoxPts.CutOffLT( pCurNode->mData.mSubDivPlane );

            MarkAllSphere( iCurNodeOffset + pCurNode->RChildOffset(), irBoxPts, irSphere, iMark );
       }
   }
}

void SpatialTreeIter::MarkAllSphereIntersect
(  
    int         iCurNodeOffset,
    BoxPts&     irBoxPts, 
    SphereSP&   irSphere, 
    tMark       iMark 
)
{
   if(      (iMark != 0x0f)
       &&   (mNodeMarks[iCurNodeOffset] & msFilterInvisible) )
   {
       return;
   }
   if(      (iMark != 0x0f)
       &&   (mNodeMarks[iCurNodeOffset] & msFilterVisible) )
   {
       return; 
       //because this chunk will always be visible, and tracked during the marksubtrees
   }

   ContiguousBinNode< SpatialNode >* pCurNode = (mpRootNode+iCurNodeOffset);

   if( !irBoxPts.mBounds.IntersectsSphere((Vector3f&)irSphere.mCenter, irSphere.mRadius) )
   {
       return;
   }

   SpatialNode* pFuckinCast;
   pFuckinCast = &(pCurNode->mData);
   mCurNodes.Add( pFuckinCast );
   mNodeMarks[iCurNodeOffset] |= iMark;

   if( pCurNode->GetSubTreeSize() == 0 ) //if leaf, marked, so leave
   {
      return;
   }

   float intersectResult = irSphere.CompareTo(pCurNode->mData.mSubDivPlane);
   if( intersectResult == 0.0f ) //intersection
   {
        //split subspace representation, and recurse
        BoxPts BoxPtsLT;
        BoxPtsLT = irBoxPts;

        BoxPtsLT.CutOffGT( pCurNode->mData.mSubDivPlane );

        MarkAllSphereIntersect( iCurNodeOffset + pCurNode->LChildOffset(), BoxPtsLT, irSphere, iMark );   

        irBoxPts.CutOffLT( pCurNode->mData.mSubDivPlane );

        MarkAllSphereIntersect( iCurNodeOffset + pCurNode->RChildOffset(), irBoxPts, irSphere, iMark );
   }
   else
   {
       if(intersectResult>0.0f) //Plane falls on the right, so recurse lt side
       {
            irBoxPts.CutOffGT( pCurNode->mData.mSubDivPlane );

            MarkAllSphereIntersect( iCurNodeOffset + pCurNode->LChildOffset(), irBoxPts, irSphere, iMark );
       }
       else //Plane falls on the left(lt), so recurse gt side
       {
            irBoxPts.CutOffLT( pCurNode->mData.mSubDivPlane );

            MarkAllSphereIntersect( iCurNodeOffset + pCurNode->RChildOffset(), irBoxPts, irSphere, iMark );
       }
   }
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void SpatialTreeIter::MarkSubTrees
(  
    int                 iCurNodeOffset, 
    BoxPts&             irBoxPts, 
    ISpatialProxyAA&    irDesiredVolume, 
    tMark               iMark 
)
{
   if(mNodeMarks[iCurNodeOffset] & iMark)
   {
       return;
   }
   ContiguousBinNode< SpatialNode >* pCurNode = (mpRootNode+iCurNodeOffset);

   SpatialNode* pFuckinCast;
   pFuckinCast = &(pCurNode->mData);
   //mCurNodes.Add( pFuckinCast );
   //mNodeMarks[iCurNodeOffset] |= iMark;

   //Test to see if the desired volume fully encompasses said subtree
   if( irDesiredVolume.CompareToXZ(irBoxPts.mBounds.mMin) <= 0.0f )
   {
       if( irDesiredVolume.CompareToXZ(irBoxPts.mBounds.mMax) <= 0.0f )
       {
           mNodeMarks[iCurNodeOffset] |= iMark;
           //It's sufficient to mark the roots of the subtrees, since queries 
           //are always performed top-down

           if(iMark & msFilterVisible)
           {
               mpCurNodeList = &mCurAlwaysVisNodes;
               MarkSubTree(iCurNodeOffset,iMark);
               mpCurNodeList = &mCurNodes;
           }
           return;
       }
   }

   if( pCurNode->GetSubTreeSize() == 0 ) //if leaf, leave
   {
      return;
   }

   float intersectResult = irDesiredVolume.CompareTo(pCurNode->mData.mSubDivPlane);
   if( intersectResult == 0.0f ) //intersection
   {
        //split subspace representation, and recurse
        BoxPts BoxPtsLT;
        BoxPtsLT = irBoxPts;

        BoxPtsLT.CutOffGT( pCurNode->mData.mSubDivPlane );

        MarkSubTrees( iCurNodeOffset + pCurNode->LChildOffset(), BoxPtsLT, irDesiredVolume, iMark );   

        irBoxPts.CutOffLT( pCurNode->mData.mSubDivPlane );

        MarkSubTrees( iCurNodeOffset + pCurNode->RChildOffset(), irBoxPts, irDesiredVolume, iMark );
   }
   else
   {
       if(intersectResult>0.0f) //Plane falls on the right, so recurse lt side
       {
            irBoxPts.CutOffGT( pCurNode->mData.mSubDivPlane );

            MarkSubTrees( iCurNodeOffset + pCurNode->LChildOffset(), irBoxPts, irDesiredVolume, iMark );
       }
       else //Plane falls on the left(lt), so recurse gt side
       {
            irBoxPts.CutOffLT( pCurNode->mData.mSubDivPlane );

            MarkSubTrees( iCurNodeOffset + pCurNode->RChildOffset(), irBoxPts, irDesiredVolume, iMark );
       }
   }
}
//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
void SpatialTreeIter::BuildBBoxes( BoxPts iBoxPts, int iCurNodeOffset )
{
   ContiguousBinNode< SpatialNode >* pCurNode = (mpRootNode+iCurNodeOffset);

   pCurNode->mData.mBBox = iBoxPts;

   if( pCurNode->GetSubTreeSize() > 0 )
   {
      iBoxPts.CutOffGT(pCurNode->mData.mSubDivPlane);
      BuildBBoxes(iBoxPts, iCurNodeOffset+pCurNode->LChildOffset());

      iBoxPts = pCurNode->mData.mBBox;
      iBoxPts.CutOffLT(pCurNode->mData.mSubDivPlane);
      BuildBBoxes(iBoxPts, iCurNodeOffset+pCurNode->RChildOffset());
   }
}

//******************************************************************************
//
// Private Member Functions
//
//******************************************************************************

#ifdef SRR2_OLD3DS_SPATIAL_WALK_FAST
void SpatialTreeIter::MarkSpherePlanes(int node, SphereSP& sphere, tMark mark)
{
    // Tail-walk one child; recurse only when a sphere spans both partitions.
    // Node insertion, mark accumulation and left-before-right order stay exact.
    for(;;)
    {
        ContiguousBinNode<SpatialNode>* current = mpRootNode + node;
        mCurNodes.Add(&current->mData);
        mNodeMarks[node] |= mark;
        if(current->GetSubTreeSize() == 0) return;
        const float side = sphere.CompareTo(current->mData.mSubDivPlane);
        if(side == 0.0f)
            MarkSpherePlanes(node + current->LChildOffset(), sphere, mark);
        node += side > 0.0f ? current->LChildOffset() : current->RChildOffset();
    }
}
#endif

#ifdef SRR2_OLD3DS_SPHERE_WALK_CACHE
void SpatialTreeIter::MarkSphereQuery(SphereSP& sphere, tMark mark)
{
    ++Old3dsSphereWalkCache::Metrics().queries;
    // Split planes are immutable after loading. Only node selection is cached;
    // every IntersectManager query still reads and tests the current entities.
    if(!Old3dsSphereWalkCache::ScalarControls() || mpRootNode->GetSubTreeSize()>=65535)
    {
        ++Old3dsSphereWalkCache::Metrics().fallback;
        MarkSpherePlanes(0,sphere,mark);
        return;
    }
    float bounds[6];
    uint32_t key[6];
    sphere.PlaneBounds(bounds);
    if(!Old3dsSphereWalkCache::MakeKey(bounds,key))
    {
        ++Old3dsSphereWalkCache::Metrics().fallback;
        MarkSpherePlanes(0,sphere,mark);
        return;
    }
    const Old3dsSphereWalkCache::Entry* cached=mSphereWalkCache.Find(key);
#ifdef SRR2_OLD3DS_SPHERE_REGION_REUSE
    if(!cached)
    {
        cached=mSphereWalkCache.FindRegion(bounds);
        if(cached) ++Old3dsSphereWalkCache::Metrics().regionHits;
    }
#endif
    if(cached)
    {
        ++Old3dsSphereWalkCache::Metrics().hits;
        Old3dsSphereWalkCache::Metrics().replayedNodes+=cached->count;
        for(unsigned i=0; i<cached->count; ++i)
        {
            const unsigned node=cached->nodes[i];
            mCurNodes.Add(&mpRootNode[node].mData);
            mNodeMarks[node]|=mark;
        }
        return;
    }
    Old3dsSphereWalkCache::Entry* record=mSphereWalkCache.Begin(key);
    MarkSphereBounds(0,bounds,mark,record);
    mSphereWalkCache.Commit(record);
    if(!record->valid) ++Old3dsSphereWalkCache::Metrics().oversized;
}

void SpatialTreeIter::MarkSphereBounds(int node, const float bounds[6], tMark mark,
                                       Old3dsSphereWalkCache::Entry* record)
{
    for(;;)
    {
        ContiguousBinNode<SpatialNode>* current=mpRootNode+node;
        mCurNodes.Add(&current->mData);
        mNodeMarks[node]|=mark;
        Old3dsSphereWalkCache::Record(record,node);
        if(current->GetSubTreeSize()==0) return;
        const AAPlane3f& plane=current->mData.mSubDivPlane;
        const int axis=plane.mAxis;
        // Keep CompareTo's upper-bound test first, including negative radii.
        const float side=plane.mPosn>bounds[axis+3] ? 1.0f :
                         plane.mPosn<bounds[axis] ? -1.0f : 0.0f;
#ifdef SRR2_OLD3DS_SPHERE_REGION_REUSE
        mSphereWalkCache.RecordPlane(axis,plane.mPosn,side);
#endif
        if(side==0.0f)
            MarkSphereBounds(node+current->LChildOffset(),bounds,mark,record);
        node+=side>0.0f ? current->LChildOffset() : current->RChildOffset();
    }
}
#endif

#ifdef SRR2_OLD3DS_SPHERE_WALK_CACHE
#include <stdio.h>
extern "C" void old3dsDumpSphereWalkCache()
{
    const Old3dsSphereWalkCache::Counters& counters=Old3dsSphereWalkCache::Metrics();
    printf("OLD3DS_SPHERE_WALK queries=%u hits=%u replayed_nodes=%u oversized=%u fallback=%u bytes_per_walker=%u\n",
           counters.queries,counters.hits,counters.replayedNodes,counters.oversized,
           counters.fallback,(unsigned)sizeof(Old3dsSphereWalkCache));
#ifdef SRR2_OLD3DS_SPHERE_REGION_REUSE
    printf("OLD3DS_SPHERE_REGION hits=%u bytes_per_walker=%u\n",
           counters.regionHits,(unsigned)sizeof(Old3dsSphereWalkCache));
#endif
}
#endif
