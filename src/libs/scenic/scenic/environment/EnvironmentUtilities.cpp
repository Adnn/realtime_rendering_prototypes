#include "EnvironmentUtilities.h"

#include "../log/Logging.h"

#include <engine/files/Loader.h>

#include <renderer/FrameBuffer.h>

#include <cassert>


// TODO #profiling
#define PROFILER_SCOPE_SINGLESHOT_SECTION(...)

namespace ad::scenic {

namespace {

    void setupCubeFiltering(const graphics::Texture & aCubemap)
    {
        glTextureParameteri(aCubemap, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(aCubemap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameterf(aCubemap, GL_TEXTURE_MAX_ANISOTROPY, 16.f);
    }


} // unnamed namespace


graphics::Texture loadCubemapFromDds(filesystem::path aDds)
{
    graphics::Texture cubemap = renderer::loadDds(aDds);
    assert(cubemap.mTarget == GL_TEXTURE_CUBE_MAP);
    setupCubeFiltering(cubemap);
    return cubemap;
}


} // namespce ad::scenic