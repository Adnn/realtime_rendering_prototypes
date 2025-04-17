#include "CameraSystem.h"


namespace ad {


void OrbitalCamera::update(int aWindowHeight)
{
    mOrbitalControl.update(mViewHeightInWorld, aWindowHeight);
}


void OrbitalCamera::setRatio(float aAspectRatio)
{
//#define ORTHO
#if defined(ORTHO)
    mCamera.setupOrthographicProjection({
        .mAspectRatio = aAspectRatio,
        .mViewHeight = mViewHeightInWorld,
        .mNearZ = -0.1f,
        .mFarZ = -20.f }
    );
#else
    mCamera.setupPerspectiveProjection({
        .mAspectRatio = aAspectRatio,
        .mVerticalFov = math::Radian{math::pi<float>/2.f},
        .mNearZ = -0.1f,
        .mFarZ = -20.f }
    );
#endif
}


scenic::GpuViewProjectionBlock OrbitalCamera::getViewProjectionBlock()
{
    mCamera.setPose(mOrbitalControl.mOrbital.getParentToLocal());
    return { mCamera };
}


} // namespace ad
