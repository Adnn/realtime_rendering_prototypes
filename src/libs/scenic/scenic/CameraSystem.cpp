#include "CameraSystem.h"


namespace ad::scenic {


void OrbitalCamera::update(int aWindowHeight)
{
    mOrbitalControl.update(mViewHeightInWorld, aWindowHeight);
}


void OrbitalCamera::reset(float aAspectRatio)
{
    //mCamera.setupOrthographicProjection({
    //    .mAspectRatio = aAspectRatio,
    //    .mViewHeight = mViewHeightInWorld,
    //    .mNearZ = -0.1f,
    //    .mFarZ = -20.f }
    //);
    mCamera.setupPerspectiveProjection({
        .mAspectRatio = aAspectRatio,
        .mVerticalFov = math::Radian{math::pi<float>/2.f},
        .mNearZ = -0.1f,
        .mFarZ = -100.f }
    );
}


scenic::GpuViewProjectionBlock OrbitalCamera::getViewProjectionBlock()
{
    mCamera.setPose(mOrbitalControl.mOrbital.getParentToLocal());
    return { mCamera };
}


} // namespace ad::scenic
