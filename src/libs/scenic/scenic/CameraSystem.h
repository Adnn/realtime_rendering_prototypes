#pragma once


#include <scenic/Camera.h>


namespace ad::scenic {


struct OrbitalCamera
{
    void update(float aDeltaTime, int aWindowHeight);

    void reset(float aAspectRatio);

    scenic::GpuViewProjectionBlock getViewProjectionBlock();

    float mViewHeightInWorld = 4.f;
    scenic::Camera mCamera;
    scenic::OrbitalControl mOrbitalControl{ scenic::Orbital{2.f} };
};


} // namespace ad::scenic
