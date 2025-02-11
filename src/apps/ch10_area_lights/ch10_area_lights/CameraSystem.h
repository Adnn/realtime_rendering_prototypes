#pragma once


#include <scenic/Camera.h>


namespace ad {

struct OrbitalCamera
{
    void update(int aWindowHeight);

    void setRatio(float aAspectRatio);

    scenic::GpuViewProjectionBlock getViewProjectionBlock();

    float mViewHeightInWorld = 4.f;
    scenic::Camera mCamera;
    scenic::OrbitalControl mOrbitalControl{ scenic::Orbital{} };
};

} // namespace ad
