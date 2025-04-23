#include "HierarchyGui.h"



namespace ad::scenic {


std::optional<Pose> presentPose(const Pose & aPose)
{
    math::EulerAngles<float> euler{math::toEulerAngles(aPose.mOrientation)};
    bool changed =
        ImGui::SliderAngle("X (roll)",  &euler.x.data(), -180, 180)
        | ImGui::SliderAngle("Y (pitch)", &euler.y.data(), -89.f, 89.f)
        | ImGui::SliderAngle("Z (yaw)",   &euler.z.data(), -180, 180)
        ;

    if (changed)
    {
        Pose result{aPose};
        result.mOrientation = toQuaternion(euler);
        return result;
    }

    return std::nullopt;
}


} // namespce ad::scenic