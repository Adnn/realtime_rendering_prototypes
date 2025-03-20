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


enum class Domain
{
    Surface,
    Volume,
};

std::vector<math::Vec<3, GLfloat>> generateUnitSphereSamples(unsigned int aCount, Domain aDomain);

void generateRandomDirections(const graphics::Texture& aDestination,
                              math::Size<2, int> aResolution);

constexpr unsigned int gSsaoSampleCount = 32;

struct FrameGraph
{
    struct SsaoControl
    {
        GLfloat mDepthBias = 0.01;
        GLfloat mSphereRadius = 0.15;
        bool mReflectSamples{ true };
        bool mWeighted{ true };
        GLfloat mWeightFactor{ 5 };
        bool mSphereInScreenSpace{ false };
    };

    FrameGraph(math::Size<2, int> aFrameSize);

    void renderDepth(const scenic::SceneTree & aSceneTree);
    void passDepth(const scenic::SceneTree & aSceneTree);

    void renderSsaoFactor(const scenic::SceneTree& aSceneTree,
                          math::Size<2, int> aRenderResolution);

    void passShowDepth(const scenic::Camera & aCamera);
    void passShowNoise();
    void passShowLinearDepth(const scenic::Camera & aCamera);

    void loadPrograms();

    void appendUi();

    struct ProgramStore
    {
        ProgramStore(Engine & aEngine);

        renderer::IntrospectProgram mDepth;
        renderer::IntrospectProgram mShowTexture;
        renderer::IntrospectProgram mShowSsao;
    };

    Engine mEngine;
    graphics::FrameBuffer mFbo;
    graphics::Texture mDepthMap;
    graphics::Texture mFragPosition_view;
    math::Size<2, int> mScreenTextureSize;
    graphics::Texture mNoiseDirections;
    ProgramStore mPrograms;
    graphics::VertexArrayObject mDummyVao;
    std::vector<math::Vec<3, GLfloat>> mSsaoSamples{
        generateUnitSphereSamples(gSsaoSampleCount, Domain::Volume)};

    SsaoControl mSsaoControl;
};


} // namespace ad