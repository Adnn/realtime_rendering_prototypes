#pragma once

#include <engine/Resources.h>

#include <engine/files/Loader.h>


namespace ad {

struct Engine
{
    renderer::Loader mLoader{ renderer::makeResourceFinder() };
};

} // namespace ad