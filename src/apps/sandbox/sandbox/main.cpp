#include "Rgba8ui.h"

#include <graphics/ApplicationGlfw.h>
#include <graphics/AppInterface.h>
#include <graphics/Timer.h>

#include <spdlog/spdlog.h>



int main(int argc, const char * argv[])
{
    try
    {
        spdlog::set_level(spdlog::level::info);

        ad::graphics::ApplicationGlfw application{
            "sandbox", 1080, 600,
            ad::graphics::ApplicationFlag::None,
            4, 6
        };
        ad::graphics::AppInterface & appInterface = *application.getAppInterface();

        // Ensures the messages are sent synchronously with the event triggering them
        // This makes debug stepping much more feasible.
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

        ad::graphics::Timer timer{glfwGetTime(), 0.};


        while(application.nextFrame())
        {
            if (appInterface.isWindowOnDisplay())
            {
                appInterface.clear();
                ad::rgba8ui::draw(appInterface.getFramebufferSize());
            }
            timer.mark(glfwGetTime());
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
