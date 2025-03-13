#pragma once

#include "ShaderConstants.h"

#include <engine/Resources.h>

#include <engine/files/Loader.h>

#include <scenic/LoadScene.h>


namespace ad {

struct Engine
{
    inline graphics::Texture loadDds(const renderer::ReferencePath& aDdsFile)
    {
        return mLoader.loadDds(aDdsFile);
    }

    inline renderer::IntrospectProgram loadProgram(const renderer::ReferencePath& aProgFile)
    {
        // Important: uses this application version of glClientConstantDefines
        return mLoader.loadProgram(aProgFile, gClientConstantDefines);
    }

    renderer::Loader mLoader{ renderer::makeResourceFinder() };
    scenic::Context mContext;
};

} // namespace ad