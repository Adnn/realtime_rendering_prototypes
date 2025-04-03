#pragma once


#include "Environment.h"

#include <engine/files/Loader.h>

#include <renderer/Texture.h>


namespace ad::scenic {


graphics::Texture loadCubemapFromDds(filesystem::path aDds);

/// @brief Cosine-lobe filtering of the hemisphere around a given normal.
/// @see rtr 4th 10.6 p424
/// @return A cubemap containing the irradiance for each direction on the sphere.
/// To be sampled with shaded fragment normal.
graphics::Texture filterEnvironmentMapDiffuse(const EnvironmentMap & aEnvMap,
                                              GLsizei aOutputSideLength,
                                              renderer::Loader & aLoader);

} // namespce ad::scenic
