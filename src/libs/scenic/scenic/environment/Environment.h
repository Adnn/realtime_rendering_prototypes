#pragma once


#include <engine/files/Loader.h>

#include <renderer/Texture.h>

#include <filesystem>


// TODO #profiling
#define PROFILER_SCOPE_SINGLESHOT_SECTION(...)
#define PROFILER_SCOPE_RECURRING_SECTION(...);


namespace ad::scenic {


inline const GLint gFilteredRadianceSide = 512;
inline const GLint gIntegratedBrdfSide = 512;
inline const GLint gFilteredIrradianceSide = 128;


struct EnvironmentMap
{
    enum class Type {
        Cubemap,
        Equirectangular,
    };


    bool isCubemap() const
    {
        return mType == Type::Cubemap;
    }

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
        GgxRadiance,
        _End/* keep last */
    };

    const EnvironmentMap & get(Category aCategory) const
    {
        switch (aCategory)
        {
        default:
            throw std::logic_error{ "Unhandled Environment::Category." };
        case EnvMap:
            return mEnvMap;
        case Irradiance:
            return mIrradianceMap;
        case GgxRadiance:
            return mGgxRadianceMap;
        }
    }

    // The "perfect mirror" map, often called environment map.
    EnvironmentMap mEnvMap;
    EnvironmentMap mIrradianceMap;
    // Outgoing radiance for a GGX specular lobe (roughness mapped to mipmap level)
    EnvironmentMap mGgxRadianceMap; 
};


std::string to_string(Environment::Category aCategory);


Environment prepareEnvironment(const renderer::ReferencePath & aEnvironmentMapPath,
                               renderer::Loader & aLoader);


} // namespce ad::scenic
