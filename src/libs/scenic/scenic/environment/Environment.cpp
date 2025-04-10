#include "Environment.h"

#include "EnvironmentUtilities.h"

namespace ad::scenic {


Environment prepareEnvironment(const renderer::ReferencePath & aEnvironmentMapPath,
                               renderer::Loader & aLoader)
{
    // TODO: extend to handle filtered maps, and support other kind of inputs
    EnvironmentMap envMap = [&]()
        {
            if (aEnvironmentMapPath.mPath.extension() == ".dds")
            {
                return EnvironmentMap{
                    .mTexture = loadCubemapFromDds(aLoader.mFinder.pathFor(aEnvironmentMapPath.mPath)),
                };
            }
            else
            {
                // We assume that if the input is not DDS, it is an equirectangular hdr image.
                assert(aEnvironmentMapPath.mPath.extension() == ".hdr");
                return EnvironmentMap{
                    .mType = EnvironmentMap::Type::Equirectangular,
                    .mTexture = loadEquirectangular(aLoader.mFinder.pathFor(aEnvironmentMapPath.mPath)),
                };
            }
        }();

    EnvironmentMap irradianceMap{
        .mTexture = filterEnvironmentMapDiffuse(envMap,
                                                gFilteredIrradianceSide,
                                                aLoader),
    };

    EnvironmentMap radianceMap{
        .mTexture = filterEnvironmentMapGgxSpecular(envMap,
                                                    gFilteredRadianceSide,
                                                    aLoader),
    };

    return{
        .mEnvMap = std::move(envMap),
        .mIrradianceMap = std::move(irradianceMap),
        .mGgxRadianceMap = std::move(radianceMap),
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
        STR(GgxRadiance);
    }
#undef STR
}


} // namespce ad::scenic