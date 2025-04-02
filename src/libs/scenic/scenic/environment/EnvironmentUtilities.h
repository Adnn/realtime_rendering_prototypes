#pragma once


#include "Environment.h"

#include <renderer/Texture.h>


namespace ad::scenic {


graphics::Texture loadCubemapFromDds(filesystem::path aDds);


} // namespce ad::scenic
