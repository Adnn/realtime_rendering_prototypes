#pragma once


#include "Engine.h"

#include <renderer/FrameBuffer.h>
#include <renderer/Shading.h>
#include <renderer/Texture.h>

#include <scenic/Model.h>


namespace ad {

namespace renderer {
    struct IntrospectProgram;
}

void drawPass(const renderer::IntrospectProgram & aProgram,
              const scenic::SceneTree& aSceneTree);


struct FrameGraph
{
    FrameGraph(math::Size<2, int> aFrameSize);

    void loadPrograms();

    void passDepth(const scenic::SceneTree & aSceneTree);

    Engine mEngine;
    graphics::FrameBuffer mFbo;
    graphics::Texture mShadowMap;
    math::Size<2, int> mShadowMapSize;
    renderer::IntrospectProgram mDepthProgram;
};


} // namespace ad