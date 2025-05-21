#pragma once


#include "Engine.h"

#include <renderer/FrameBuffer.h>
#include <renderer/Shading.h>
#include <renderer/Texture.h>

#include <scenic/Model.h>

#include <scenic/environment/Environment.h>


namespace ad {

namespace renderer {
    struct IntrospectProgram;
}

namespace scenic {
    class Camera;
    struct Environment;
}

void drawPass(const renderer::IntrospectProgram & aProgram,
              const scenic::SceneTree& aSceneTree);


enum class Domain
{
    Surface,
    Volume,
};

std::vector<math::Vec<3, GLfloat>> generateUnitSphereSamples_carthesian(unsigned int aCount, Domain aDomain);
std::vector<math::Vec<3, GLfloat>> generateUnitSphereSamples_spherical(unsigned int aCount, Domain aDomain);
std::vector<math::Vec<3, GLfloat>> generateHemisphereSample(unsigned int aCount);
std::vector<math::Vec<3, GLfloat>> generateHemisphereSample_importance(unsigned int aCount,
                                                                       bool aWeightDistance,
                                                                       bool aWeightCosine,
                                                                       float aDistanceFactor);

void generateRandomDirections(const graphics::Texture& aDestination,
                              math::Size<2, int> aResolution);

constexpr unsigned int gSsaoSampleCount = 128;

struct TextureStore
{
    TextureStore(math::Size<2, int> aFrameSize);

    enum Name {
        DepthMap,
        FragPositionView,
        FragNormalView,
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

    // TODO: get better automation for enumerations
    // (even if they cannot be language "enum").
    inline static const std::vector<Name> gNames{
        DepthMap,
        FragPositionView,
        FragNormalView,
        RawOcclusion,
        FilteredOcclusion,
    };

    struct Data
    {
        graphics::Texture mTexture;
        Mode mMode;
    };

    void setupTexture(Name aName, GLenum aInternalFormat, GLenum aWrapMode);

    std::vector<Data> mStore;
    math::Size<2, int> mScreenTextureSize;
};


std::string to_string(TextureStore::Name aName);


struct FrameGraph
{

    struct FrameControl
    {
        inline static constexpr std::array<GLenum, 3> gPolygonModes{
             GL_POINT,
             GL_LINE,
             GL_FILL,
         }; 

        decltype(gPolygonModes)::const_iterator mPolygonMode = gPolygonModes.begin() + 2;
        bool mApplyAo{ true };
        bool mApplyEnvironment{ true };
        scenic::Environment::Category mSkyboxCategory{ scenic::Environment::EnvMap };
        GLfloat mSkyboxLodBias = 0.f;

        GLfloat mSpecularIblFactor = 1.f;
        GLfloat mDiffuseIblFactor = 1.f;
    };

    struct SphereSsaoControl
    {
        GLfloat mDepthBias = 0.01;
        GLfloat mSphereRadius = 0.15;
        bool mReflectSamples{ true };
        bool mWeighted{ true };
        GLfloat mWeightFactor{ 5 };
        bool mSphereInScreenSpace{ false };
        bool mScreenSpaceNonLinearDepth{ false };
    };

    struct HemiSsaoControl
    {
        GLfloat mDepthBias = 0.01;
        GLfloat mSphereRadius = 0.15;
        bool mImportanceSampling{ true };
        bool mRotateSamples{ true };
        bool mWeightDistance{ true };
        bool mWeightCosine{ false };
        GLfloat mDistanceFactor{ 5 };
    };

    struct BlurControl
    {
        GLint mBlurRadius = 2;
        GLfloat mDepthFactor = 10;
        GLfloat mNormalFactor = 10;
    };

    FrameGraph(math::Size<2, int> aFrameSize);

    void resizeFrame(math::Size<2, int> aRenderResolution);

    void renderSimple(const scenic::SceneTree & aSceneTree);

    void renderFrame(const scenic::SceneTree & aSceneTree,
                     const scenic::Environment & aEnvironment);

    void renderFragPosition(const scenic::SceneTree & aSceneTree);
    void passFragPosition(const scenic::SceneTree & aSceneTree);

    void passSphereSsaoFactor(const scenic::SceneTree& aSceneTree,
                              math::Size<2, int> aRenderResolution);

    void passHemisphereSsaoFactor(const scenic::SceneTree& aSceneTree,
                                 math::Size<2, int> aRenderResolution);

    void passFilterAo(math::Size<2, int> aRenderResolution);

    void passBlinnPhong(const scenic::SceneTree & aSceneTree);

    void passForwardPbr(const scenic::SceneTree& aSceneTree,
                        const scenic::Environment & aEnvironment,
                        math::Size<2, int> aRenderResolution);

    void passSkybox(const scenic::Environment & aEnvironment);

    void passShowNoise();

    void passShowTexture(const scenic::Camera& aCamera,
                         TextureStore::Name aName);

    void loadPrograms();

    void appendUi();

    const graphics::Texture & tex(TextureStore::Name aName) const
    {
        return mTextures.mStore.at(aName).mTexture;
    }

    struct ProgramStore
    {
        ProgramStore(Engine & aEngine);

        renderer::IntrospectProgram mDepth;
        renderer::IntrospectProgram mShowTexture;
        renderer::IntrospectProgram mSphereSsao;
        renderer::IntrospectProgram mHemisphereSsao;
        renderer::IntrospectProgram mBlinnPhong;
        renderer::IntrospectProgram mBlurTexture;
        renderer::IntrospectProgram mForwardPbr;
        renderer::IntrospectProgram mSkyboxCubemap;
        renderer::IntrospectProgram mSkyboxEquirectangular;
    };

    enum class SsaoMethod
    {
        Sphere,
        OrientedHemishphere,
        _End/* Keep last */
    };

    Engine mEngine;
    graphics::FrameBuffer mFbo;
    TextureStore mTextures;
    // TODO: move to texture store
    graphics::Texture mNoiseDirections;
    // The second part of the split integral approximation for IBL
    graphics::Texture mIntegratedGgxBrdf;
    // TODO: we could render directly to the default framebuffer, but currently we blit
    graphics::Texture mFinalFrame;
    ProgramStore mPrograms;
    graphics::VertexArrayObject mDummyVao;

    SsaoMethod mSsaoMethod = SsaoMethod::OrientedHemishphere;
    SphereSsaoControl mSphereSsaoControl;
    HemiSsaoControl mHemisphereSsaoControl;
    BlurControl mBlurControl;
    FrameControl mFrameControl;

    std::vector<math::Vec<3, GLfloat>> mSphereSamples{
        generateUnitSphereSamples_spherical(gSsaoSampleCount, Domain::Volume)};
    std::vector<math::Vec<3, GLfloat>> mHemisphereSamples{
        generateHemisphereSample(gSsaoSampleCount)};
    std::vector<math::Vec<3, GLfloat>> mHemisphereSamples_importance{
        generateHemisphereSample_importance(gSsaoSampleCount,
                                            mHemisphereSsaoControl.mWeightDistance,
                                            mHemisphereSsaoControl.mWeightCosine,
                                            mHemisphereSsaoControl.mDistanceFactor)};
};


inline std::string to_string(FrameGraph::SsaoMethod aMethod)
{
    switch (aMethod)
    {
    default:
        throw std::logic_error{ "Unhandled FrameGraph::SsaoMethod." };
    case FrameGraph::SsaoMethod::Sphere:
        return "Sphere";
    case FrameGraph::SsaoMethod::OrientedHemishphere:
        return "OrientedHemishphere";
    }
}


} // namespace ad