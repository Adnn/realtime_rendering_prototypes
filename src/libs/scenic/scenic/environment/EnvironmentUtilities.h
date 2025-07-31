#pragma once


#include "Environment.h"

#include <engine/files/Loader.h>

#include <math/Homogeneous.h>

#include <renderer/Texture.h>


namespace ad::scenic {


/// @brief The orientation matrices for a cubemap, in the OpenGL order 
///
/// The matrices will rotate +X, -X, +Y, -Y, +Z, -Z onto -Z (camera forward)
/// as well as negating the Y axis (cubemap are top-left origin, see .cpp)
extern const std::array<math::AffineMatrix<4, float>, 6> gCubeCaptureViewsNegateY;

graphics::Texture loadCubemapFromDds(std::filesystem::path aDds);

graphics::Texture loadEquirectangular(std::filesystem::path aEquirectangularMap);


/// @brief Notably useful to transform an equirectangular map to a cubemap
graphics::Texture renderToCubemap(const EnvironmentMap & aEnvMap,
                                  GLsizei aOutputSideLength,
                                  GLint aTextureLevels,
                                  renderer::Loader & aLoader);

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
