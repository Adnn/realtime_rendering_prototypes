#pragma once

#include "FrameGraph.h"

#include <engine/Lights.h>

#include <renderer/UniformBuffer.h>


namespace ad {


struct Shadow
{
    Shadow();

    void renderShadowMaps(const scenic::SceneTree & aSceneTree,
                          const renderer::LightsDataCommon & mLights,
                          FrameGraph & aGraph);


    graphics::UniformBufferObject mLightViewBuffer;
    graphics::UniformBufferObject mCubeFacesViewBuffer;
};


} // namespace ad