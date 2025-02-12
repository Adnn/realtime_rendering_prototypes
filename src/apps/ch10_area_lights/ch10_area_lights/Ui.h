#pragma once

#include <imgui.h>


namespace ad {


class Ui
{
public:
    void present(const char * aName)
    {
        ImGui::Begin(aName);

        ImGui::Checkbox("Show ImGui demo", &mShowDemo);
        if (mShowDemo)
        {
            ImGui::ShowDemoWindow(&mShowDemo);
        }

        ImGui::End();
    }

private:
    bool mShowDemo = false;
};


} // namespace ad
