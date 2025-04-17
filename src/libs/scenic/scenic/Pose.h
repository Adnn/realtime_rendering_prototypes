#pragma once


#include <math/Homogeneous.h>
#include <math/Quaternion.h>
#include <math/Vector.h>


namespace ad::scenic {


struct Pose
{
    // Assuming `this` Pose represents a Pose in space A (i.e., local-to-A transform, from `this` perspective).
    // Given the Pose of a child Node (from the child perspective, its local-to-parent),
    // this functions returns the child Pose in space A  (i.e. local-to-A transform, from the child perspective).
    // If space A is canonical, it allows to recursively get absolute Pose for each Node in a tree.
    Pose transform(Pose aNested) const
    {
        // TODO #math Is it correct? When it is, this should be moved to the math library
        aNested.mPosition = mPosition + mOrientation.rotate(mUniformScale * aNested.mPosition);
        aNested.mUniformScale *= mUniformScale;
        aNested.mOrientation = mOrientation * aNested.mOrientation;
        return aNested;
    }

    explicit operator math::AffineMatrix<4, float> () const;

    bool operator==(const Pose &) const = default;

    // Position is modeled as a Vec, because in a graph it can be seen as relative displacements.
    math::Vec<3, float> mPosition;
    // TODO #scaling #skew Should we allow non-uniform (3D) scaling?
    // That would allow skewing, making decomposition unpractical
    float mUniformScale{1.f};
    math::Quaternion<float> mOrientation = math::Quaternion<float>::Identity();
};


Pose interpolate(const Pose & aLeft, const Pose & aRight, float aInterpolant);


// TODO #math #linearalgebra move to a more generic library (graphics?)
Pose decompose(const math::AffineMatrix<4, float>& aTransformation);


inline Pose composeLeftToRight(Pose aNested, const Pose& aParent)
{
    return aParent.transform(aNested);
}


} // namespce ad::scenic
