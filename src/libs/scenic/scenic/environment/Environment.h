#pragma once


#include <engine/files/Loader.h>

#include <renderer/Texture.h>

#include <filesystem>


// TODO #profiling
#define PROFILER_SCOPE_SINGLESHOT_SECTION(...)
#define PROFILER_SCOPE_RECURRING_SECTION(...);


namespace ad::scenic {


struct EnvironmentMap
{
    enum class Type {
        Cubemap,
    };

    // We might want to extend to support equirectangular
    Type mType{ Type::Cubemap };
    graphics::Texture mTexture;
};

struct Environment
{
    enum Category 
    {
        EnvMap,
        Irradiance,
        _End/* keep last */
    };

    const EnvironmentMap & get(Category aCategory) const
    {
        switch (aCategory)
        {
        case EnvMap:
            return mEnvMap;
        case Irradiance:
            return mIrradianceMap;
        }
    }

    // The "perfect mirror" map, often called environment map.
    EnvironmentMap mEnvMap;
    EnvironmentMap mIrradianceMap;
};


std::string to_string(Environment::Category aCategory);


Environment prepareEnvironment(const renderer::ReferencePath & aEnvironmentMapPath,
                               renderer::Loader & aLoader);


} // namespce ad::scenic
