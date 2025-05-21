#include "Scene.h"
#include "Ui.h"
#include "log/Logging.h"

#include <graphics/ApplicationGlfw.h>
#include <graphics/AppInterface.h>
#include <graphics/Timer.h>

#include <ui/ImguiUi.h>

#include <spdlog/spdlog.h>


// TODOS:
// * Merge back serializeTexture()
// * Histogram computation for textures content (compute shaders)
// * Depth texture linearization for display
// * Render SSAO pass without rendering geometry (with the position buffer instead)
// * Bilateralize the filter
// * Attempt to accept larger noise textures
// * Provide the viewport and the samples in UBOs

int main(int argc, const char* argv[])
{
    try
    {
        spdlog::set_level(spdlog::level::debug);

        glfwWindowHint(GLFW_CONTEXT_ROBUSTNESS, GLFW_LOSE_CONTEXT_ON_RESET);
        ad::graphics::ApplicationGlfw application("ch11_voxel", 1080, 600,
                                                  ad::graphics::ApplicationFlag::None,
                                                  4, 6,
                                                  { {GLFW_CONTEXT_ROBUSTNESS, GLFW_LOSE_CONTEXT_ON_RESET} });

        // Sanity checks: the context is robust and lose context as requested
        {
            GLint contextFlags = 0;
            glGetIntegerv(GL_CONTEXT_FLAGS, &contextFlags);
            assert(contextFlags & GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT);
            glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY, &contextFlags);
            assert(contextFlags & GL_LOSE_CONTEXT_ON_RESET);
        }

        // This ensures the messages are sent synchronously with the event triggering them
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
            scene.step(timer, application.getAppInterface()->getWindowSize());
            scene.render(application.getAppInterface()->getFramebufferSize());
            timer.mark(glfwGetTime());

            ad::imguiui::newFrame();
            ui.present("Root", scene);
            ad::imguiui::renderFrame();

            // If an error occurs, such as infinite loop in a shader causing the driver to timeout
            // it seems to only be catched at this point, not immediately after the triggering drawcall.
            // TODO: Avoid terminate(). The problem is that stack unwinding calls destructors,
            // and some try to destruct GL objects thus failing if the context is not valid.
            GLenum resetStatus = glGetGraphicsResetStatus();
            if (resetStatus != GL_NO_ERROR)
            {
                if (resetStatus == GL_GUILTY_CONTEXT_RESET)
                {
                    ADLOG_THROW(critical, "OpenGL: Guilty context reset (likely caused by the application).");
                    std::terminate();
                }
                else if (resetStatus == GL_INNOCENT_CONTEXT_RESET)
                {
                    ADLOG_THROW(critical, "OpenGL: Innocent context reset (external cause).");
                    std::terminate();
                }
                else if (resetStatus == GL_UNKNOWN_CONTEXT_RESET)
                {
                    ADLOG_THROW(critical, "OpenGL: Unknown context reset (cause undetermined).");
                    std::terminate();
                }
            }
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

