#include "CameraGui.h"

#include <imgui.h>


namespace ad::scenic {


namespace {

    // GUI related values
    constexpr float gMinZ{-0.01f};
    constexpr float gMaxZ{-1000.f};
    constexpr float gMinViewHeight{0.01f};
    constexpr float gMaxViewHeight{1000.f};


    bool appendUi(graphics::PerspectiveParameters & aParams)
    {
        bool changed = false;

        ImGui::Text("Aspect ratio: %f", aParams.mAspectRatio);
        changed |= ImGui::SliderAngle("Vertical FOV", &aParams.mVerticalFov.data(), 0.f, 180.f);
        changed |= ImGui::InputFloat("Near Z", &aParams.mNearZ, 0.01f, .5f, "%.2f");
        changed |= ImGui::InputFloat("Far Z", &aParams.mFarZ, 0.01f, .5f, "%.2f");
        if(changed)
        {
            aParams.mNearZ = std::clamp(aParams.mNearZ, gMaxZ - gMinZ, gMinZ); 
            aParams.mFarZ = std::clamp(aParams.mFarZ, gMaxZ, aParams.mNearZ + gMinZ); 
        }

        return changed;
    }


    bool appendUi(graphics::OrthographicParameters & aParams)
    {
        bool changed = false;

        ImGui::Text("Aspect ratio: %f", aParams.mAspectRatio);
        // Note: Cannot put it in a single instruction 
        // because shortcut evaluation would stop drawing subsequent widgets
        changed |= ImGui::InputFloat("View height", &aParams.mViewHeight, 0.1f, 1.f, "%.2f");
        changed |= ImGui::InputFloat("Near Z", &aParams.mNearZ, 0.01f, .5f, "%.2f");
        changed |= ImGui::InputFloat("Far Z", &aParams.mFarZ, 0.01f, .5f, "%.2f");
        if(changed)
        {
            aParams.mViewHeight = std::clamp(aParams.mViewHeight, gMinViewHeight, gMaxViewHeight); 
            aParams.mNearZ = std::clamp(aParams.mNearZ, gMaxZ - gMinZ, gMinZ); 
            aParams.mFarZ = std::clamp(aParams.mFarZ, gMaxZ, aParams.mNearZ + gMinZ); 
        }

        return changed;
    }


} // unnamed namespace


void appendUi(OrbitalCamera & aCameraSystem)
{
     // Projection
    {
        scenic::Camera & camera = aCameraSystem.mCamera;

        ImGui::SeparatorText("Projection");

        // Control the projection type
        Camera::Projection projectionId = camera.identifyProjection();
        bool changed = ImGui::RadioButton("Orthographic", (int*)&projectionId, Camera::Orthographic);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Perspective", (int*)&projectionId, Camera::Perspective);
        // If a change occured, convert from one projection parameter type to the other
        if(changed)
        {
            // Even if this is not the active control mode, we use orbital to determine the plane distance
            const float radius = aCameraSystem.mOrbitalControl.mOrbital.mSpherical.radius();
            camera.switchProjection(projectionId, radius);
        }

        // Edit the current projection parameters
        std::visit([& camera = aCameraSystem.mCamera](auto params)
        {
            if (appendUi(params))
            {
                camera.setupProjection(params);
            }
        }, camera.getProjectionParameters());
    }
}


} // namespace ad::scenic