#include "CameraSystem.h"


namespace ad {


void OrbitalCamera::update(float aDeltaTime, int aWindowHeight)
{
    mOrbitalControl.update(aDeltaTime, mViewHeightInWorld, aWindowHeight);
}


void OrbitalCamera::setRatio(float aAspectRatio)
{
    //mCamera.setupOrthographicProjection({
    //    .mAspectRatio = aAspectRatio,
    //    .mViewHeight = mViewHeightInWorld,
    //    .mNearZ = -0.1f,
    //    .mFarZ = -20.f }
    //);
    mCamera.setupPerspectiveProjection({
        .mAspectRatio = aAspectRatio,
        .mVerticalFov = math::Radian<float>{math::pi<float>/2.f},
        .mNearZ = -0.1f,
        .mFarZ = -20.f }
    );
}


scenic::GpuViewProjectionBlock OrbitalCamera::getViewProjectionBlock()
{
    mCamera.setPose(mOrbitalControl.mOrbital.getParentToLocal());
    return { mCamera };
}


} // namespace ad
