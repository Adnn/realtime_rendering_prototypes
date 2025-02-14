#include "Logging-init.h"

#include "Logging.h"

#include <spdlog/sinks/stdout_color_sinks.h>


//
// On MSVC
//
#if defined(_MSC_VER) && !defined(__llvm__)

namespace ad::renderer {


LoggerInitialization::LoggerInitialization()
{
    spdlog::stdout_color_mt(gMainLogger);
    spdlog::stdout_color_mt(gPipelineDiag);
}


const LoggerInitialization LoggerInitialization::gInitialized;


} // namespace ad::renderer


void ad_renderer_loggerinitialization()
{
    // The hack to keep the symbol `LoggerInitialization::gInitialized`
    // works even though we are not accessing it.
}



//
// On Clang or GCC
//
#else

namespace ad::renderer {


std::pair<std::shared_ptr<spdlog::logger>, std::shared_ptr<spdlog::logger>>
initializeLogger()
{
    return std::make_pair(spdlog::stdout_color_mt(gMainLogger),
                          spdlog::stdout_color_mt(gPipelineDiag));
}


} // namespace ad::renderer

#endif