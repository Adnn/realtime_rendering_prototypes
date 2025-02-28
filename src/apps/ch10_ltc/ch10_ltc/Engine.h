#pragma once

#include "ShaderConstants.h"

#include <engine/Resources.h>

#include <engine/files/Loader.h>


namespace ad {

class Engine
{
public:
    inline renderer::IntrospectProgram loadProgram(const renderer::ReferencePath& aProgFile)
    {
        // Important: uses this application version of glClientConstantDefines
        return mLoader.loadProgram(aProgFile, gClientConstantDefines);
    }

private:
    renderer::Loader mLoader{ renderer::makeResourceFinder() };
};

} // namespace ad