#pragma once


#include <assimp/scene.h>

#include <math/Box.h>
#include <math/Matrix.h>
#include <math/Quaternion.h>
#include <math/Vector.h>

#include <cassert>


namespace ad::scenic {


inline bool hasTrianglesOnly(aiMesh * aMesh)
{
    //TODO Ad 2023/07/21: Understand what is this NGON encoding flag for.
    return (aMesh->mPrimitiveTypes = (aiPrimitiveType_TRIANGLE | aiPrimitiveType_NGONEncodingFlag));
}


inline math::AffineMatrix<4, float> extractAffinePart(const aiMatrix4x4 & aAiMatrix)
{
    auto m = aAiMatrix;
    assert(m.d1 == 0 && m.d2 == 0 && m.d3 == 0 && m.d4 == 1);
    return math::AffineMatrix<4, float>{ math::Matrix<4, 3, float>{
        m.a1, m.b1, m.c1,
        m.a2, m.b2, m.c2,
        m.a3, m.b3, m.c3,
        m.a4, m.b4, m.c4,
    } };
}


inline math::AffineMatrix<4, float> extractAffinePart(const aiNode * aNode)
{
    return extractAffinePart(aNode->mTransformation);
}


inline math::Position<3, float> toPosition(aiVector3D aVec)
{
    return {aVec.x, aVec.y, aVec.z};
}


inline math::Vec<3, float> toVec(aiVector3D aVec)
{
    return {aVec.x, aVec.y, aVec.z};
}


inline math::Quaternion<float> toQuaternion(aiQuaternion aQuat)
{
    return {aQuat.x, aQuat.y, aQuat.z, aQuat.w};
}


inline math::Box<float> extractAabb(const aiMesh * aMesh)
{
    aiAABB aiBox = aMesh->mAABB;
    return{
        .mPosition = toPosition(aiBox.mMin),
        .mDimension = (toPosition(aiBox.mMax) - toPosition(aiBox.mMin)).as<math::Size>(),
    };
}


} // namespace ad::scenic