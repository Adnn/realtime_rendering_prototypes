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

namespace scenic {
    class Camera;
}

void drawPass(const renderer::IntrospectProgram & aProgram,
              const scenic::SceneTree& aSceneTree);


struct FrameGraph
{
    FrameGraph(math::Size<2, int> aFrameSize);

    void renderDepth(const scenic::SceneTree & aSceneTree);
    void passDepth(const scenic::SceneTree & aSceneTree);

    void passShowDepth(const scenic::Camera & aCamera);

    void loadPrograms();

    struct ProgramStore
    {
        ProgramStore(Engine & aEngine);

        renderer::IntrospectProgram mDepth;
        renderer::IntrospectProgram mShowTexture;
    };

    Engine mEngine;
    graphics::FrameBuffer mFbo;
    graphics::Texture mShadowMap;
    math::Size<2, int> mShadowMapSize;
    ProgramStore mPrograms;
    graphics::VertexArrayObject mDummyVao;
};


} // namespace ad