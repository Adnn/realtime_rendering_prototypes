#pragma once


//
// On MSVC
//
#if defined(_MSC_VER) && !defined(__llvm__)

namespace ad::renderer {


class LoggerInitialization
{
private:
    // __declspec is not forcing the export for static libs, apparently.
    /*__declspec(dllexport)*/ static const LoggerInitialization gInitialized;
    LoggerInitialization();
};


} // namespace ad::renderer


extern "C"
{
void ad_renderer_loggerinitialization();
}


//
// On Clang or GCC
//
#else

#include <memory>
#include <spdlog/logger.h>
#include <utility>


namespace ad::renderer {


std::tuple<std::shared_ptr<spdlog::logger>>
initializeLogger();

struct LoggerInitialization
{

    inline static const std::tuple<std::shared_ptr<spdlog::logger>>
        gLogger = initializeLogger();
};


} // namespace ad::renderer

#endif