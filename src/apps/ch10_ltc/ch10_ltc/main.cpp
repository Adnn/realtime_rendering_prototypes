#include "Scene.h"
#include "Ui.h"

#include <graphics/ApplicationGlfw.h>
#include <graphics/AppInterface.h>
#include <graphics/Timer.h>

#include <ui/ImguiUi.h>

#include <spdlog/spdlog.h>


// TODOS:
// * Implement spotlight
// * Handle the auto registration of loggers
// * Load / reload from prog files, from asset folders
//   * Keep an history of compiled programs
// * Offer checkbox to auto-reload on file change vs. explicit button
// * Use StringIds for Semantic
// * Offer color buffer (all texture type?) comparison
//   * Show / write image diff
//   * Offer side-by-side & toggle between images

int main(int argc, const char * argv[])
{
    try
    {
        spdlog::set_level(spdlog::level::debug);

        ad::graphics::ApplicationGlfw application("ch10_ltc", 1080, 600,
                                                  ad::graphics::ApplicationFlag::None,
                                                  4, 6);
        glClearColor(1.f, 1.f, 1.f, 1.f);

        // Ensures the messages are sent synchronously with the event triggering them
        // This makes debug stepping much more feasible.
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

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
    catch(...)
    {
        std::cerr << "Non-standard exception reaching top level."
                  << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::exit(EXIT_SUCCESS);
}
