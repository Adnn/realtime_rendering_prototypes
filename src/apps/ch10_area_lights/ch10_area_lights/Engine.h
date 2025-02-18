#pragma once

#include <engine/Resources.h>
#include <engine/ShaderConstants.h>

#include <engine/files/Loader.h>


namespace ad {

class Engine
{
public:
    inline renderer::IntrospectProgram loadProgram(const renderer::ReferencePath& aProgFile)
    {
        return mLoader.loadProgram(aProgFile, renderer::gClientConstantDefines);
    }

private:
    renderer::Loader mLoader{ renderer::makeResourceFinder() };
};

} // namespace ad