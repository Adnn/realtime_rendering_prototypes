#pragma once

#include <imgui.h>


namespace ad::graphics {


class ApplicationGlfw;


} // namespace ad::graphics


namespace ad::imguiui {


class ImguiUi
{
public:
    ImguiUi(const graphics::ApplicationGlfw& aGlfwApp);
    ~ImguiUi();

    bool isCapturingKeyboard() const;
    bool isCapturingMouse() const;
};


void initImgui(const graphics::ApplicationGlfw & aGlfwApp);

void terminate();

void newFrame();

void renderFrame();


} // namespace ad::imguiui
