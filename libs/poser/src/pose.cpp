#ifdef SRR2_OLD3DS_MAIN_PROFILE
#include <main/old3dsmainprofile.h>
#else
#define O3_MAIN_SCOPE(stage) ((void)0)
#endif
/*
 * poser/pose.cpp
 */

#include <poser/pose.hpp>
#include <poser/joint.hpp>
#ifdef SRR2_OLD3DS_POSE_REUSE
#include <string.h>
#endif

#include <p3d/error.hpp>
#include <p3d/anim/skeleton.hpp>
#include <p3d/anim/pose.hpp>


namespace poser
{


Pose::Pose():
    m_Skeleton(0),
    m_Joints(0),
    m_Children(0)
{
}

Pose::Pose(tSkeleton* skeleton)
{
    P3DASSERT(skeleton != 0);
    
    if (skeleton != 0)
    {
        m_Skeleton = skeleton;
        m_Skeleton->AddRef();
        
        int jointCount = m_Skeleton->GetNumJoint();
        m_Joints = new Joint [jointCount];
        m_Children = new Joint* [jointCount];
        
        int j;
        // count child joints
        for (j = 1; j < jointCount; ++j)
        {
            m_Joints[j].SetIndex(j);

            int parentIndex = m_Skeleton->GetJoint(j)->parentIndex;
            P3DASSERT((parentIndex >= 0) && (parentIndex < j));
            Joint* parentJoint = (m_Joints + parentIndex);
            m_Joints[j].SetParentJoint(parentJoint);
            parentJoint->SetChildJointCount(parentJoint->GetChildJointCount() + 1);
        }
        
        int useChild = 0;
        for (j = 0; j < jointCount; ++j)
        {
            if(m_Joints[j].GetChildJointCount())
            {
                int old = m_Joints[j].GetChildJointCount();
                m_Joints[j].SetChildJointMem(&m_Children[useChild]);
                useChild += old ;
            }
        }

        // update parent child joint list
        for (j = 1; j < jointCount; ++j)
        {
            int parentIndex = m_Skeleton->GetJoint(j)->parentIndex;
            P3DASSERT((parentIndex >= 0) && (parentIndex < j));
            Joint* parentJoint = (m_Joints + parentIndex);
            parentJoint->AddChildJoint(m_Joints + j);
        }
    }
    else
    {
        m_Skeleton = 0;
        m_Joints = 0;
    }
}

Pose::Pose(Pose* pose):
    m_Skeleton(0),
    m_Joints(0)
{
    Assign(pose);
}

Pose::Pose(tPose* p3dPose):
    m_Skeleton(0),
    m_Joints(0)
{
    Assign(p3dPose);
}

Pose::~Pose()
{
    delete[] m_Joints;
    delete[] m_Children;
    
    if (m_Skeleton != 0)
    {
        m_Skeleton->Release();
    }
}

void Pose::Assign(Pose* pose)
{
#ifdef SRR2_OLD3DS_POSE_REUSE
    m_ReuseWorlds = false;
#endif
    tSkeleton *skel = pose->m_Skeleton;
    
    if (skel != m_Skeleton)
    {
        tRefCounted::Assign(m_Skeleton, skel);
        
        delete[] m_Joints;
        int jointCount = m_Skeleton->GetNumJoint();
        
        if (jointCount > 0)
        {
            m_Joints = new Joint [jointCount];
            m_Children = new Joint* [jointCount];
            
            m_Joints[0] = pose->m_Joints[0];
            
            int j;
            
            // count child joints & assign
            for (j = 1; j < jointCount; ++j)
            {
                m_Joints[j].SetIndex(j);

                int parentIndex = m_Skeleton->GetJoint(j)->parentIndex;
                P3DASSERT((parentIndex >= 0) && (parentIndex < j));
                Joint* parentJoint = (m_Joints + parentIndex);
                m_Joints[j].SetParentJoint(parentJoint);
                parentJoint->SetChildJointCount(parentJoint->GetChildJointCount() + 1);
                m_Joints[j] = pose->m_Joints[j];
            }
            
            int useChild = 0;
            for (j = 0; j < jointCount; ++j)
            {
                if(m_Joints[j].GetChildJointCount())
                {
                    int old = m_Joints[j].GetChildJointCount();
                    m_Joints[j].SetChildJointMem(&m_Children[useChild]);
                    useChild += old ;
                }
            }

            // update parent child joint list
            for (j = 1; j < jointCount; ++j)
            {
                int parentIndex = m_Skeleton->GetJoint(j)->parentIndex;
                P3DASSERT((parentIndex >= 0) && (parentIndex < j));
                Joint* parentJoint = (m_Joints + parentIndex);
                parentJoint->AddChildJoint(m_Joints + j);
            }
        }
        else
        {
            m_Joints = 0;
        }
    }
    else
    {
        int jointCount = GetJointCount();
        
        for (int j = 0; j < jointCount; ++j)
        {
            m_Joints[j] = pose->m_Joints[j];
        }
    }
}

void Pose::Assign(tPose* p3dPose)
{
    O3_MAIN_SCOPE(PoserAssign);
    tSkeleton *skel = p3dPose->GetSkeleton();
    
    if (skel != m_Skeleton)
    {
        tRefCounted::Assign(m_Skeleton, skel);
        
        delete[] m_Joints;
        int jointCount = m_Skeleton->GetNumJoint();
        
        if (jointCount > 0)
        {
            m_Joints = new Joint [jointCount];
            m_Children = new Joint* [jointCount];
            
            m_Joints[0].SetObjectTransform(Transform(p3dPose->GetJoint(0)->objectMatrix));
            
            int j;
            
            // count parent child joints
            for (j = 1; j < jointCount; ++j)
            {
                m_Joints[j].SetIndex(j);

                int parentIndex = m_Skeleton->GetJoint(j)->parentIndex;
                P3DASSERT((parentIndex >= 0) && (parentIndex < j));
                Joint* parentJoint = (m_Joints + parentIndex);
                m_Joints[j].SetParentJoint(parentJoint);
                parentJoint->SetChildJointCount(parentJoint->GetChildJointCount() + 1);
                
                m_Joints[j].SetObjectTransform(Transform(p3dPose->GetJoint(j)->objectMatrix));
            }
            
            int useChild = 0;
            for (j = 0; j < jointCount; ++j)
            {
                if(m_Joints[j].GetChildJointCount())
                {
                    int old = m_Joints[j].GetChildJointCount();
                    m_Joints[j].SetChildJointMem(&m_Children[useChild]);
                    useChild += old ;
                }
            }

            // update parent child joints
            for (j = 1; j < jointCount; ++j)
            {
                int parentIndex = m_Skeleton->GetJoint(j)->parentIndex;
                P3DASSERT((parentIndex >= 0) && (parentIndex < j));
                Joint* parentJoint = (m_Joints + parentIndex);
                parentJoint->AddChildJoint(m_Joints + j);
            }
        }
        else
        {
            m_Joints = 0;
        }
    }
    else
    {
        int jointCount = GetJointCount();
    
        // initialize pose
        for (int i = 0; i < jointCount; ++i)
        {
#ifdef SRR2_OLD3DS_POSE_REUSE
            Joint& joint = m_Joints[i];
            const rmt::Matrix& object = p3dPose->GetJoint(i)->objectMatrix;
            // Root world is retained by the reference assignment and its getter.
            // Other worlds depend on the full local matrix and their parent's world.
            // Parent-first skeleton order makes invalidation transitive without a cache.
            const bool parentDirty = joint.m_ParentJoint &&
                joint.m_ParentJoint->m_ParentJoint &&
                joint.m_ParentJoint->m_Dirty == Joint::DIRTY_WORLD;
            // Direct world writes followed by inverse-local conversion can round.
            // Only a previously evaluated local/parent product proves reuse safe.
            const bool worldProductValid = !joint.m_ParentJoint
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
                || joint.m_WorldCoherent
#endif
                ;
            const bool retainWorld = worldProductValid && m_ReuseWorlds && joint.m_Dirty == Joint::DIRTY_NONE && !parentDirty &&
                memcmp(&joint.m_Object.GetMatrix(), &object, sizeof(object)) == 0;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
            const bool parentRotationDirty=joint.m_ParentJoint &&
                joint.m_ParentJoint->m_ParentJoint &&
                joint.m_ParentJoint->m_Dirty==Joint::DIRTY_WORLD &&
                !joint.m_ParentJoint->m_RotationCoherent;
            joint.m_RotationCoherent=joint.m_RotationCoherent && m_ReuseWorlds && joint.m_Dirty==Joint::DIRTY_NONE &&
                !parentRotationDirty && Joint::RotationReuseAllowed() && Joint::SameRotation(joint.m_Object.GetMatrix(),object);
#endif
            // Keep the reference quaternion invalidation even when the matrix is equal.
            joint.m_Object.SetMatrix(object);
            if(!retainWorld) joint.m_Dirty = Joint::DIRTY_WORLD;
            else if(joint.m_ParentJoint)
            {
                // Reference world evaluation also invalidates a cached quaternion.
                // Keep that canonical matrix-derived representation without multiplying.
                joint.m_World.SetMatrix(joint.m_World.GetMatrix());
            }
#else
            m_Joints[i].m_Object.SetMatrix(p3dPose->GetJoint(i)->objectMatrix);
            m_Joints[i].m_Dirty = Joint::DIRTY_WORLD;
#endif
        }
    }
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
#ifdef SRR2_OLD3DS_JOINT_COHERENCE_CHAIN
    // Retain an existing product proof; a clean direct-world write alone does
    // not prove that inverse-local/parent multiplication reproduces its bits.
    // Preserve that proof for unchanged setters in the following driver pass.
    for(int i = 0; i < GetJointCount(); ++i)
        m_Joints[i].m_WorldCoherent = m_Joints[i].m_WorldCoherent &&
            m_Joints[i].m_Dirty == Joint::DIRTY_NONE;
#else
    for(int i = 0; i < GetJointCount(); ++i) m_Joints[i].m_WorldCoherent = false;
#endif
#endif
#ifdef SRR2_OLD3DS_POSE_REUSE
    m_ReuseWorlds = true;
#endif
}

int Pose::GetJointCount() const
{
    if (m_Skeleton == 0)
        return 0;
    return m_Skeleton->GetNumJoint();
}

void Pose::SetRestPose()
{
    int jointCount = GetJointCount();
    
    // initialize pose
    for (int i = 0; i < jointCount; ++i)
    {
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
        m_Joints[i].m_WorldCoherent = false;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
        m_Joints[i].m_RotationCoherent = false;
#endif
#endif
        m_Joints[i].m_Object.SetMatrix(m_Skeleton->GetJoint(i)->restPose);
        m_Joints[i].m_Dirty = Joint::DIRTY_WORLD;
    }

}

void Pose::FillP3DPose(tPose* p3dPose, bool fillObjectMatrices) const
{
    O3_MAIN_SCOPE(PoserFill);
    // make sure these are compatible poses
    // Not no more we dont (nbrooke 29/1/03)
//    P3DASSERT(p3dPose->GetSkeleton() == m_Skeleton);
//    if (p3dPose->GetSkeleton() != m_Skeleton)
//        return;
    
    int jointCount = GetJointCount();
    
    if (fillObjectMatrices)
    {
        // update output pose
        for (int i = 0; i < jointCount; ++i)
        {
            // update pose joint
            const Joint* joint = (m_Joints + i);
            tPose::Joint* p3dJoint = p3dPose->GetJoint(i);
        
            p3dJoint->objectMatrix = joint->GetObjectTransform().GetMatrix();
            p3dJoint->worldMatrix = joint->GetWorldTransform().GetMatrix();
        }
    }
    else
    {
        // update output pose
        for (int i = 0; i < rmt::Min(jointCount, p3dPose->GetNumJoint());  ++i)
        {
            // update pose joint
            const Joint* joint = (m_Joints + i);
            tPose::Joint* p3dJoint = p3dPose->GetJoint(i);
        
            p3dJoint->worldMatrix = joint->GetWorldTransform().GetMatrix();
        }
    }

    p3dPose->SetPoseReady(true);
}

void Pose::Clean()
{
#ifdef SRR2_OLD3DS_POSE_REUSE
    m_ReuseWorlds = false;
#endif
    int jointCount = GetJointCount();
    for (int i = 0; i < jointCount; ++i)
    {
        m_Joints[i].Clean();
    }
}


} // namespace poser


// End of file.
