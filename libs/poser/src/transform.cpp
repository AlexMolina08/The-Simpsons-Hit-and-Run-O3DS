/*
 * poser/transform.cpp
 */

#include <poser/transform.hpp>
#ifdef SRR2_OLD3DS_QUATERNION_REUSE
#include <string.h>
#endif

namespace poser
{


Transform::Transform()
{
    Identity();
}

Transform::Transform(const rmt::Matrix& m)
{
    m_Matrix = m;
    m_Quaternion.Identity();
    m_Dirty = DIRTY_QUATERNION;
}

Transform::Transform(const rmt::Quaternion& q,
                     const rmt::Vector& v)
{
    m_Matrix.Identity();
    m_Matrix.FillTranslate(v);
#ifdef SRR2_OLD3DS_QUATERNION_REUSE
    m_Dirty = DIRTY_QUATERNION;
#endif
    SetQuaternion(q);
}

Transform::Transform(const rmt::Vector& v)
{
    Identity();
    m_Matrix.FillTranslate(v);
}

void Transform::Identity()
{
    m_Matrix.Identity();
    m_Quaternion.Identity();
    m_Dirty = DIRTY_NONE;
}

void Transform::SetMatrix(const rmt::Matrix& m)
{
#ifdef SRR2_OLD3DS_QUATERNION_REUSE
    // Only BuildFromMatrix produces this canonical representation. Quaternions
    // supplied by drivers may encode the same rotation with the opposite sign.
    const bool retain = m_Dirty == DIRTY_NONE_CANONICAL &&
        memcmp(m_Matrix.m[0], m.m[0], 3 * sizeof(float)) == 0 &&
        memcmp(m_Matrix.m[1], m.m[1], 3 * sizeof(float)) == 0 &&
        memcmp(m_Matrix.m[2], m.m[2], 3 * sizeof(float)) == 0;
#endif
    m_Matrix = m;
#ifdef SRR2_OLD3DS_QUATERNION_REUSE
    m_Dirty = retain ? DIRTY_NONE_CANONICAL : DIRTY_QUATERNION;
#else
    m_Dirty = DIRTY_QUATERNION;
#endif
}

void Transform::SetMatrixRotation(const rmt::Matrix& m)
{
#ifdef SRR2_OLD3DS_QUATERNION_REUSE
    // Only BuildFromMatrix produces this canonical representation. Quaternions
    // supplied by drivers may encode the same rotation with the opposite sign.
    const bool retain = m_Dirty == DIRTY_NONE_CANONICAL &&
        memcmp(m_Matrix.m[0], m.m[0], 3 * sizeof(float)) == 0 &&
        memcmp(m_Matrix.m[1], m.m[1], 3 * sizeof(float)) == 0 &&
        memcmp(m_Matrix.m[2], m.m[2], 3 * sizeof(float)) == 0;
#endif
    m_Matrix.Row(0) = m.Row(0);
    m_Matrix.Row(1) = m.Row(1);
    m_Matrix.Row(2) = m.Row(2);
#ifdef SRR2_OLD3DS_QUATERNION_REUSE
    m_Dirty = retain ? DIRTY_NONE_CANONICAL : DIRTY_QUATERNION;
#else
    m_Dirty = DIRTY_QUATERNION;
#endif
}

const rmt::Quaternion& Transform::GetQuaternion() const
{
    if (m_Dirty == DIRTY_QUATERNION)
    {
        Transform* cthis = const_cast<Transform*>(this);
        cthis->m_Quaternion.BuildFromMatrix(cthis->m_Matrix);
#ifdef SRR2_OLD3DS_QUATERNION_REUSE
        cthis->m_Dirty = DIRTY_NONE_CANONICAL;
#else
        cthis->m_Dirty = DIRTY_NONE;
#endif
    }
    
    return m_Quaternion;
}

void Transform::SetQuaternion(const rmt::Quaternion& q)
{
#ifdef SRR2_OLD3DS_QUATERNION_REUSE
    // DIRTY_NONE identifies a driver quaternion with its matching converted
    // matrix (or Identity). A matrix-derived quaternion has separate provenance.
    if(m_Dirty == DIRTY_NONE && memcmp(&m_Quaternion, &q, sizeof(q)) == 0)
        return;
#endif
    m_Quaternion = q;
    m_Quaternion.ConvertToMatrix(&m_Matrix);
    m_Dirty = DIRTY_NONE;
}

void Transform::Blend(float alpha, const Transform& xForm)
{
    // blend translation
    rmt::Vector blendVector = GetTranslation();
    blendVector.Scale(1.0f - alpha);
    
    rmt::Vector otherVector = xForm.GetTranslation();
    otherVector.Scale(alpha);
    
    blendVector.Add(otherVector);
    SetTranslation(blendVector);
    
    // blend rotation
    rmt::Quaternion blendQuat;
    blendQuat.Slerp(GetQuaternion(), xForm.GetQuaternion(), alpha);
    SetQuaternion(blendQuat);
}

void Transform::Invert()
{
    m_Matrix.InvertOrtho();
    m_Dirty = DIRTY_QUATERNION;
}


} // namespace poser


// End of file.
