#pragma once


#include "../IntrospectProgram.h"

#include <renderer/Texture.h>

#include <resource/ResourceFinder.h> 

#include <filesystem>


namespace ad::renderer {


// TODO Ad: should it be made part of handy::resource API directly?
/// \brief A strong-typedef around filesystem::path to document when an API expects
/// a path to be resolved via resources prefixes.
struct ReferencePath
{
    explicit ReferencePath(std::filesystem::path aPath) :
        mPath{ std::move(aPath) }
    {}

    std::filesystem::path mPath;
};


// TODO: Move to a lower-level library (e.g. renderer/DdsGL.h)
graphics::Texture loadDds(const std::filesystem::path & aDds);

enum class ColorSpace
{
    Linear,
    sRGB,
};

graphics::Texture loadTexture(const std::filesystem::path & aImagePath,
                              ColorSpace aSourceColorSpace);


struct Loader
{
    graphics::Texture loadDds(const ReferencePath & aDdsFile);

    /// @brief Load a `.prog` file as an IntrospectProgram.
    IntrospectProgram loadProgram(const ReferencePath & aProgFile,
                                  std::vector<graphics::MacroDefine> aDefines = {}) const;

    graphics::ShaderSource loadShader(const ReferencePath & aShaderFile) const;

    resource::ResourceFinder mFinder;
};


} // namespace ad::renderer