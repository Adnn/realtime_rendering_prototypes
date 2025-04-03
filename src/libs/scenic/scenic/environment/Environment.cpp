#include "Environment.h"

#include "EnvironmentUtilities.h"

namespace ad::scenic {

//
// Setup the filtered environment textures
//
const GLint gFilteredRadianceSide = 512;
const GLint gIntegratedBrdfSide = 512;
const GLint gFilteredIrradianceSide = 128;


Environment prepareEnvironment(const renderer::ReferencePath & aEnvironmentMapPath,
                               renderer::Loader & aLoader)
{
    // TODO: extend to handle filtered maps, and support other kind of inputs
    // (equirectangular, non-DDS, ...)
    assert(aEnvironmentMapPath.mPath.extension() == ".dds");
    EnvironmentMap envMap{
        .mTexture = loadCubemapFromDds(aLoader.mFinder.pathFor(aEnvironmentMapPath.mPath)),
    };

    EnvironmentMap irradianceMap{
        .mTexture = filterEnvironmentMapDiffuse(envMap,
                                                gFilteredIrradianceSide,
                                                aLoader),
    };

    return{
        .mEnvMap = std::move(envMap),
        .mIrradianceMap = std::move(irradianceMap),
    };
}


std::string to_string(Environment::Category aCategory)
{
#define STR(enumerator) case Environment::##enumerator: return #enumerator
    switch (aCategory)
    {
        default:
            throw std::logic_error{ "Unhandled Environment::Category." };
        STR(EnvMap);
        STR(Irradiance);
    }
#undef STR
}


} // namespce ad::scenic