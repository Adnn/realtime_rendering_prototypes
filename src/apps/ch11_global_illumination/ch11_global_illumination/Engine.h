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

    inline renderer::IntrospectProgram loadProgram(const renderer::ReferencePath& aProgFile,
                                                   // By value as we will mutate
                                                   std::vector<graphics::MacroDefine> aClientDefines = {})
    {
        // Important: uses this application version of glClientConstantDefines
        aClientDefines.insert(aClientDefines.end(),
                              gClientConstantDefines.begin(), gClientConstantDefines.end());

        return mLoader.loadProgram(aProgFile, aClientDefines);
    }

    renderer::Loader mLoader{ renderer::makeResourceFinder() };
    scenic::Context mContext;
};

} // namespace ad