#pragma once

#include <resource/ResourceFinder.h>


namespace ad::renderer {


/// @brief Make a resource finder rooted at the calling executable folder.
/// If executable folder contains asset.json, use the listed prefixes instead.
resource::ResourceFinder makeResourceFinder();


} // namespace ad::renderer