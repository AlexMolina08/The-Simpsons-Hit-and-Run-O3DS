//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


/*
 * poser/joint.cpp
 */

#include <poser/joint.hpp>

#include <p3d/error.hpp>
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
#include <string.h>
#endif


#if defined(SRR2_OLD3DS_JOINT_ROTATION_REUSE) && defined(RAD_3DS)
#include <atomic>
#include <stdio.h>
static std::atomic<bool> rotationReuseValidated(false);
static bool RotationReuseActive()
{
    if(!rotationReuseValidated.load(std::memory_order_relaxed))return false;
    unsigned control;
    __asm__ volatile("vmrs %0, fpscr":"=r"(control));
    return (control&0x03f79f00u)==0x03000000u;
}
#endif

namespace poser
{


Joint::Joint():
    m_Index(0),
    m_ParentJoint(0),
    m_MaxChildJointCount(0),
    m_ChildJointCount(0),
    m_ChildJoints(0),
    m_Dirty(DIRTY_NONE)
{
}

Joint::Joint(const Joint& joint):
    m_Index(0),
    m_ParentJoint(0),
    m_MaxChildJointCount(0),
    m_ChildJointCount(0),
    m_ChildJoints(0)
{
    *this = joint;
}

Joint::~Joint()
{
}

Joint& Joint::operator = (const Joint& joint)
{
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    m_WorldCoherent = false;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    m_RotationCoherent = false;
#endif
#endif
    m_Object = joint.m_Object;
    m_World = joint.m_World;
    m_Dirty = joint.m_Dirty;
    
    return *this;
}

void Joint::SetChildJointCount(int count)
{
    P3DASSERT(m_ChildJoints == 0);
    m_ChildJointCount = count;
}

void Joint::SetChildJointMem(Joint** mem)
{
    m_ChildJoints = mem;
    m_ChildJointCount = 0;
}

Joint* Joint::GetChildJoint(int i)
{
    P3DASSERT((i >= 0) && (i < m_ChildJointCount));
    if ((i < 0) || (i >= m_ChildJointCount))
        return 0;
    
    return m_ChildJoints[i];
}

const Joint* Joint::GetChildJoint(int i) const
{
    P3DASSERT((i >= 0) && (i < m_ChildJointCount));
    if ((i < 0) || (i >= m_ChildJointCount))
        return 0;
    
    return m_ChildJoints[i];
}

bool Joint::AddChildJoint(Joint* child)
{
    /*
    // allocate array, if necessary
    if (m_ChildJoints == 0)
    {
        P3DASSERT(m_ChildJointCount > 0);
        m_ChildJoints = new Joint* [m_ChildJointCount];
        m_ChildJointCount = 0;
    }
    */

    // append child joint pointer
    m_ChildJoints[m_ChildJointCount] = child;
    ++m_ChildJointCount;
    
    return true;
}

const Transform& Joint::GetObjectTransform() const
{
    if (m_Dirty == DIRTY_OBJECT)
    {
        Joint* cthis = const_cast<Joint*>(this);
        
        if (m_ParentJoint != 0)
        {
            Transform invPWorld = m_ParentJoint->GetWorldTransform();
            invPWorld.Invert();
            
            cthis->m_Object = m_World;
            cthis->m_Object.Mult(invPWorld);
        }
        
        cthis->m_Dirty = DIRTY_NONE;
    }
    
    return m_Object;
}

void Joint::SetObjectTransform(const Transform& xForm)
{
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    m_WorldCoherent = false;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    m_RotationCoherent = false;
#endif
#endif
    m_Object = xForm;
    m_Dirty = DIRTY_WORLD;
    NotifyWorldModified();
}

void Joint::SetObjectMatrix(const rmt::Matrix& matrix)
{
    GetObjectTransform();
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    m_RotationCoherent = m_RotationCoherent && SameRotation(m_Object.GetMatrix(),matrix);
#endif
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    m_WorldCoherent = m_WorldCoherent && memcmp(&m_Object.GetMatrix(), &matrix, sizeof(matrix)) == 0;
#endif
    m_Object.SetMatrix(matrix);
    m_Dirty = DIRTY_WORLD;
    NotifyWorldModified(
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
        m_WorldCoherent
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
        , m_RotationCoherent
#endif
#else
        false
#endif
        );
}

void Joint::SetObjectMatrixRotation(const rmt::Matrix& matrix)
{
    GetObjectTransform();
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    m_RotationCoherent = m_RotationCoherent && SameRotation(m_Object.GetMatrix(),matrix);
#endif
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    m_WorldCoherent = m_WorldCoherent && memcmp(m_Object.GetMatrix().m[0], matrix.m[0], 12) == 0 &&
        memcmp(m_Object.GetMatrix().m[1], matrix.m[1], 12) == 0 &&
        memcmp(m_Object.GetMatrix().m[2], matrix.m[2], 12) == 0;
#endif
    m_Object.SetMatrixRotation(matrix);
    m_Dirty = DIRTY_WORLD;
    NotifyWorldModified(
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
        m_WorldCoherent
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
        , m_RotationCoherent
#endif
#else
        false
#endif
        );
}

void Joint::SetObjectTranslation(const rmt::Vector& trans)
{
    GetObjectTransform();
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    m_WorldCoherent = m_WorldCoherent && memcmp(&m_Object.GetTranslation(), &trans, sizeof(trans)) == 0;
#endif
    m_Object.SetTranslation(trans);
    m_Dirty = DIRTY_WORLD;
    NotifyWorldModified(
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
        m_WorldCoherent
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
        , m_RotationCoherent
#endif
#else
        false
#endif
        );
}

void Joint::SetObjectQuaternion(const rmt::Quaternion& quat)
{
    GetObjectTransform();
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    const bool coherent = m_WorldCoherent;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    const bool rotationCoherent=m_RotationCoherent && RotationReuseAllowed();
#endif
    rmt::Matrix previous;
    if(coherent
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
       || rotationCoherent
#endif
      ) previous = m_Object.GetMatrix();
#endif
    m_Object.SetQuaternion(quat);
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    m_WorldCoherent = coherent && memcmp(&previous, &m_Object.GetMatrix(), sizeof(previous)) == 0;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    // Drivers often write the same rotation while a parent only translates.
    // Keep the existing product proof when the converted nine floats match.
    m_RotationCoherent=rotationCoherent && SameRotation(previous,m_Object.GetMatrix());
#endif
#endif
    m_Dirty = DIRTY_WORLD;
    NotifyWorldModified(
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
        m_WorldCoherent
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
        , m_RotationCoherent
#endif
#else
        false
#endif
        );
}

const Transform& Joint::GetWorldTransform() const
{
    if (m_Dirty == DIRTY_WORLD)
    {
        Joint* cthis = const_cast<Joint*>(this);
        
        if (m_ParentJoint != 0)
        {
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
            const Transform& parent = m_ParentJoint->GetWorldTransform();
            const bool unchanged = m_WorldCoherent;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
            const bool rotationUnchanged=m_RotationCoherent;
#endif
            if(unchanged)
            {
                // Reference multiplication makes the world quaternion matrix-derived.
                cthis->m_World.SetMatrix(cthis->m_World.GetMatrix());
            }
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
            else if(rotationUnchanged
#ifdef RAD_3DS
                    && RotationReuseActive()
#endif
                    )
            {
                // Preserve the reference affine product's order and quaternion
                // provenance. Only the unchanged rotation products are omitted.
                rmt::Matrix result=m_World.GetMatrix();
                const rmt::Matrix& a=m_Object.GetMatrix();
                const rmt::Matrix& b=parent.GetMatrix();
                MultiplyTranslation(result,a,b);
                cthis->m_World.SetMatrix(result);
            }
#endif
            else
            {
                cthis->m_World = m_Object;
                cthis->m_World.Mult(parent);
            }
            cthis->m_WorldCoherent = true;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
            cthis->m_RotationCoherent=(!unchanged || rotationUnchanged) &&
#ifdef RAD_3DS
                RotationReuseActive();
#else
                true;
#endif
#endif
            NotifyWorldModified(unchanged
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
                ,rotationUnchanged
#endif
                );
#else
            cthis->m_World = m_Object;
            cthis->m_World.Mult(m_ParentJoint->GetWorldTransform());
            
            NotifyWorldModified();
#endif
        }
        
        cthis->m_Dirty = DIRTY_NONE;
    }
    
    return m_World;
}

void Joint::SetWorldTransform(const Transform& xForm)
{
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    const bool unchanged = m_ChildJointCount > 0 &&
        memcmp(&m_World.GetMatrix(), &xForm.GetMatrix(), sizeof(rmt::Matrix)) == 0;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    const bool rotationUnchanged = RotationReuseAllowed() && m_ChildJointCount>0 && SameRotation(m_World.GetMatrix(),xForm.GetMatrix());
#endif
    m_WorldCoherent = false;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    m_RotationCoherent = false;
#endif
#endif
    m_World = xForm;
    m_Dirty = DIRTY_OBJECT;
    NotifyWorldModified(
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
        unchanged
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
        , rotationUnchanged
#endif
#else
        false
#endif
        );
}

void Joint::NotifyWorldModified(bool unchanged
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    , bool rotationUnchanged
#endif
    ) const
{
    for (int i = 0; i < m_ChildJointCount; ++i)
    {
        m_ChildJoints[i]->ParentWorldModified(unchanged
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
            ,rotationUnchanged
#endif
            );
    }
}

void Joint::ParentWorldModified(bool unchanged
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    , bool rotationUnchanged
#endif
    )
{
    (void)unchanged;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    if(!rotationUnchanged)m_RotationCoherent=false;
#endif
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    if(!unchanged) m_WorldCoherent = false;
#endif
#if (defined(RAD_DEBUG) || defined(RAD_TUNE))
    // It is a driver ordering error if an object matrix
    // is dirty when its parent joint's world matrix has
    // been modified!
    if (m_Dirty == DIRTY_OBJECT)
    {
        P3DASSERT("poseEngine::Joint error!  Child object transform dirty "
                  "during parent world transform modification.  Your "
                  "PoseDrivers are probably sorted incorrectly." == 0);
        return;
    }
#endif
    
    if (m_Dirty != DIRTY_NONE)
        return;
    
    m_Dirty = DIRTY_WORLD;
    
    for (int i = 0; i < m_ChildJointCount; ++i)
    {
        m_ChildJoints[i]->ParentWorldModified(unchanged
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
            ,rotationUnchanged
#endif
            );
    }
}

void Joint::SetWorldMatrix(const rmt::Matrix& matrix)
{
    GetWorldTransform();
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    const bool unchanged = memcmp(&m_World.GetMatrix(), &matrix, sizeof(matrix)) == 0;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    const bool rotationUnchanged = RotationReuseAllowed() && SameRotation(m_World.GetMatrix(),matrix);
#endif
    m_WorldCoherent = false;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    m_RotationCoherent = false;
#endif
#endif
    m_World.SetMatrix(matrix);
    m_Dirty = DIRTY_OBJECT;
    NotifyWorldModified(
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
        unchanged
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
        , rotationUnchanged
#endif
#else
        false
#endif
        );
}

void Joint::SetWorldMatrixRotation(const rmt::Matrix& matrix)
{
    GetWorldTransform();
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    const bool unchanged = memcmp(m_World.GetMatrix().m[0], matrix.m[0], 12) == 0 &&
        memcmp(m_World.GetMatrix().m[1], matrix.m[1], 12) == 0 &&
        memcmp(m_World.GetMatrix().m[2], matrix.m[2], 12) == 0;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    const bool rotationUnchanged = unchanged;
#endif
    m_WorldCoherent = false;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    m_RotationCoherent = false;
#endif
#endif
    m_World.SetMatrixRotation(matrix);
    m_Dirty = DIRTY_OBJECT;
    NotifyWorldModified(
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
        unchanged
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
        , rotationUnchanged
#endif
#else
        false
#endif
        );
}

void Joint::SetWorldTranslation(const rmt::Vector& trans)
{
    GetWorldTransform();
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    const bool unchanged = memcmp(&m_World.GetTranslation(), &trans, sizeof(trans)) == 0;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    const bool rotationUnchanged = true;
#endif
    m_WorldCoherent = false;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    m_RotationCoherent = false;
#endif
#endif
    m_World.SetTranslation(trans);
    m_Dirty = DIRTY_OBJECT;
    NotifyWorldModified(
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
        unchanged
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
        , rotationUnchanged
#endif
#else
        false
#endif
        );
}

void Joint::SetWorldQuaternion(const rmt::Quaternion& quat)
{
    GetWorldTransform();
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    m_WorldCoherent = false;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    m_RotationCoherent = false;
#endif
#endif
    m_World.SetQuaternion(quat);
    m_Dirty = DIRTY_OBJECT;
    NotifyWorldModified();
}

void Joint::Clean()
{
#ifdef SRR2_OLD3DS_JOINT_WORLD_REUSE
    m_WorldCoherent = false;
#ifdef SRR2_OLD3DS_JOINT_ROTATION_REUSE
    m_RotationCoherent = false;
#endif
#endif
    m_Dirty = DIRTY_NONE;
}



#if defined(SRR2_OLD3DS_JOINT_ROTATION_REUSE) && defined(RAD_3DS)
bool Joint::NativeRotationReuseAllowed(){return rotationReuseValidated.load(std::memory_order_relaxed);}
void Joint::ValidateRotationReuse()
{
    unsigned initial;
    __asm__ volatile("vmrs %0, fpscr":"=r"(initial));
    unsigned state=0x81c74a39u, mismatches=0, products=0;
    if((initial&0x03f79f00u)==0x03000000u)
    {
        for(unsigned n=0;n<16384;++n)
        {
            rmt::Matrix a,b,cached,reference;
            for(unsigned i=0;i<4;++i)for(unsigned j=0;j<3;++j)
            {
                state^=state<<13;state^=state>>17;state^=state<<5;
                unsigned bits=(state&0x807fffffu)|((117+state%20)<<23);
                memcpy(&a.m[i][j],&bits,4);
                state^=state<<13;state^=state>>17;state^=state<<5;
                bits=(state&0x807fffffu)|((117+state%20)<<23);
                memcpy(&b.m[i][j],&bits,4);
            }
            a.IdentityProjection();b.IdentityProjection();
            cached.Mult(a,b);
            a.m[3][0]=float(int(n)-8192)*0.03125f;
            b.m[3][2]=float(int(n)-4096)*0.0625f;
            reference.Mult(a,b);
            MultiplyTranslation(cached,a,b);
            mismatches+=memcmp(&cached,&reference,sizeof(cached))!=0;
            ++products;
        }
    }
    rotationReuseValidated.store(products==16384 && mismatches==0,std::memory_order_relaxed);
    printf("OLD3DS_JOINT_ROTATION_CHECK products=%u mismatches=%u enabled=%u fpscr=%08x\n",
        products,mismatches,rotationReuseValidated.load(std::memory_order_relaxed),initial);
    __asm__ volatile("vmsr fpscr, %0"::"r"(initial):"memory");
}
extern "C" void old3dsValidateJointRotationReuse(){Joint::ValidateRotationReuse();}
#endif

} // namespace poser


// End of file.
