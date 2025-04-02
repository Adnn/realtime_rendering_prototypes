#pragma once


#include <renderer/Texture.h>

#include <filesystem>


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
    // The "perfect mirror" map, often called environment map.
    EnvironmentMap mEnvMap;
};


Environment prepareEnvironment(const std::filesystem::path & aEnvironmentMapPath);


} // namespce ad::scenic
