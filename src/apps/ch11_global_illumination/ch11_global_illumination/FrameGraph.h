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

struct TextureStore
{
    enum Name {
        DepthMap,
        FragPositionView,
        RawOcclusion,
        FilteredOcclusion,
        _End/*keep last*/
    };

    enum Mode : unsigned int {
        LINEARIZE_DEPTH = 1u,
        DIRECTION = 2u,
        DEPTH_FROM_POSITION = 3u,
        RAW_RED_CHANNEL = 4u,
    };

    std::vector<graphics::Texture> mStore;
    math::Size<2, int> mScreenTextureSize;
};


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
    
    struct BlurControl
    {
        GLint mBlurRadius = 4;
    };

    FrameGraph(math::Size<2, int> aFrameSize);

    void renderFrame(const scenic::SceneTree& aSceneTree,
                     math::Size<2, int> aRenderResolution);

    void renderFragPosition(const scenic::SceneTree & aSceneTree);
    void passFragPosition(const scenic::SceneTree & aSceneTree);

    void passSsaoFactor(const scenic::SceneTree& aSceneTree,
                        math::Size<2, int> aRenderResolution);

    void passFilterAo(math::Size<2, int> aRenderResolution);

    void passShowDepth(const scenic::Camera & aCamera);
    void passShowNoise();
    void passShowLinearDepth(const scenic::Camera & aCamera);

    void passShowTexture(const scenic::Camera& aCamera,
                         TextureStore::Name aName,
                         TextureStore::Mode aMode);

    void loadPrograms();

    void appendUi();

    const graphics::Texture & tex(TextureStore::Name aName) const
    {
        return mTextures.mStore.at(aName);
    }

    struct ProgramStore
    {
        ProgramStore(Engine & aEngine);

        renderer::IntrospectProgram mDepth;
        renderer::IntrospectProgram mShowTexture;
        renderer::IntrospectProgram mShowSsao;
        renderer::IntrospectProgram mBlurTexture;
    };

    Engine mEngine;
    graphics::FrameBuffer mFbo;
    TextureStore mTextures;
    // TODO: move to texture store
    graphics::Texture mNoiseDirections;
    ProgramStore mPrograms;
    graphics::VertexArrayObject mDummyVao;
    std::vector<math::Vec<3, GLfloat>> mSsaoSamples{
        generateUnitSphereSamples(gSsaoSampleCount, Domain::Volume)};

    SsaoControl mSsaoControl;
    BlurControl mBlurControl;
};


} // namespace ad