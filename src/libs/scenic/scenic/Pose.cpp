#include "Pose.h"

#include <math/Transformations.h>
#include <math/Interpolation/Interpolation.h>
#include <math/Interpolation/QuaternionInterpolation.h>

#include <cassert>


namespace ad::scenic {


Pose::operator math::AffineMatrix<4, float> () const
{
    return math::trans3d::scaleUniform(mUniformScale) 
        * mOrientation.toRotationMatrix()
        * math::trans3d::translate(mPosition);
}


Pose interpolate(const Pose & aLeft, const Pose & aRight, float aInterpolant)
{
    return Pose{
        .mPosition     = math::lerp(aLeft.mPosition, aRight.mPosition, aInterpolant),
        .mUniformScale = math::lerp(aLeft.mUniformScale, aRight.mUniformScale, aInterpolant),
        .mOrientation  = math::slerp(aLeft.mOrientation, aRight.mOrientation, aInterpolant),
    };
}

Pose decompose(const math::AffineMatrix<4, float> & aTransformation)
{
    // TODO Ad 2024/03/27: #pose This is limited to transformation matrices with positive (no mirroring)
    // and uniform (no shearing) scaling.
    // For a more general approach, notably see graphics gems II VII.1 
    // "decomposing ecomposing a matrix into simple matrix into simple transformations"

    Pose result {
        .mPosition = aTransformation.getAffine(),
    };

    std::array<math::Vec<3, float>, 3> rows{
        math::Vec<3, float>{aTransformation[0]},
        math::Vec<3, float>{aTransformation[1]},
        math::Vec<3, float>{aTransformation[2]},
    };
    math::Size<3, float> scale{
        rows[0].getNorm(),
        rows[1].getNorm(),
        rows[2].getNorm(),
    };

    // Assert scaling is uniform
    assert(math::relativeTolerance(scale[1], scale[0], 1E-6f));
    assert(math::relativeTolerance(scale[2], scale[0], 1E-6f));
    result.mUniformScale = scale[0];

    math::LinearMatrix<3, 3, float> rotationMatrix{
        aTransformation[0][0] / scale[0], aTransformation[0][1] / scale[0], aTransformation[0][2] / scale[0],
        aTransformation[1][0] / scale[1], aTransformation[1][1] / scale[1], aTransformation[1][2] / scale[1],
        aTransformation[2][0] / scale[2], aTransformation[2][1] / scale[2], aTransformation[2][2] / scale[2],
    };
    result.mOrientation = math::toQuaternion(rotationMatrix);

    return result;
}

} // namespce ad::scenic