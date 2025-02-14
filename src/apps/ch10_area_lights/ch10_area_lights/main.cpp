#include "Scene.h"
#include "Ui.h"

#include <graphics/ApplicationGlfw.h>
#include <graphics/AppInterface.h>
#include <graphics/Timer.h>

#include <ui/ImguiUi.h>

#include <spdlog/spdlog.h>


int main(int argc, const char * argv[])
{
    try
    {
        spdlog::set_level(spdlog::level::debug);

        ad::graphics::ApplicationGlfw application("ch10_area_lights", 800, 600);

        ad::imguiui::ImguiUi imgui{application};
        ad::Ui ui;

        ad::graphics::Timer timer{glfwGetTime(), 0.};

        ad::Scene scene{
            *application.getAppInterface(),
            imgui
        };

        while(application.nextFrame())
        {
            application.getAppInterface()->clear();
            scene.step(timer, application.getAppInterface()->getWindowSize());
            scene.render(application.getAppInterface()->getFramebufferSize());
            timer.mark(glfwGetTime());

            ad::imguiui::newFrame();
            ui.present("Root", scene);
            ad::imguiui::renderFrame();
        }
    }
    catch(const std::exception & e)
    {
        std::cerr << "Exception:\n"
                  << e.what()
                  << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::exit(EXIT_SUCCESS);
}
