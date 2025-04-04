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


/// @brief Implement the 1st part of the split-sum approximation
/// @return An environment map representing the radiance along an outgoing direction
/// corresponding to the sampled principal incoming direction.
/// (principal incoming direction might just be view reflection, or a skew of it.)
graphics::Texture filterEnvironmentMapGgxSpecular(const EnvironmentMap& aEnvMap,
                                                  GLsizei aOutputSideLength,
                                                  renderer::Loader & aLoader);


/// @brief Implement the 2nd part (scale & bias to F0) of the split-sum approximation
graphics::Texture integrateEnvironmentBrdf(GLsizei aOutputSideLength,
                                           renderer::Loader & aLoader);

} // namespce ad::scenic
