#include "Environment.h"

#include "EnvironmentUtilities.h"

namespace ad::scenic {


Environment prepareEnvironment(const std::filesystem::path & aEnvironmentMapPath)
{
    // TODO: extend to handle filtered maps, and support other kind of inputs
    // (equirectangular, non-DDS, ...)
    assert(aEnvironmentMapPath.extension() == ".dds");
    return {
        .mEnvMap{
            .mTexture{loadCubemapFromDds(aEnvironmentMapPath)}
    }};
}


} // namespce ad::scenic