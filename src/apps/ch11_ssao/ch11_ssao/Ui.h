#pragma once

#include <imgui.h>

#include "Scene.h"

namespace ad {


class Ui
{
public:
    void present(const char * aName, Scene & aScene)
    {
        ImGui::Begin(aName);

        ImGui::Checkbox("Show ImGui demo", &mShowDemo);
        if (mShowDemo)
        {
            ImGui::ShowDemoWindow(&mShowDemo);
        }

        ImGui::Checkbox("Show Scene", &mShowScene);
        if (mShowScene)
        {
            aScene.presentUi(&mShowScene);
        }


        ImGui::End();
    }

private:
    bool mShowDemo = false;
    bool mShowScene = true;
};


} // namespace ad
