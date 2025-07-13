#pragma once


#include "Engine.h"

#include <renderer/UniformBuffer.h>
#include <renderer/FrameBuffer.h>
#include <renderer/Shading.h>
#include <renderer/Texture.h>

#include <scenic/Model.h>

#include <scenic/environment/Environment.h>


namespace ad {

void drawPass(const renderer::IntrospectProgram & aProgram,
              const scenic::SceneTree & aSceneTree,
              const Engine & aEngine);


struct Voxelizer;


struct FrameGraph
{
    static constexpr float gShadowCubeNearDistance = 0.01;
    static constexpr float gShadowCubeFarDistance = 100;

    struct FrameControl
    {
        inline static constexpr std::array<GLenum, 3> gPolygonModes{
             GL_POINT,
             GL_LINE,
             GL_FILL,
         }; 

        enum class ToneMapping : GLuint 
        {
            None,
            Reinhard,
            Aces,
            AcesApprox,
            _End/* keep last */
        };

        enum class ShadowMethod : GLuint
        {
            ShadowMap,
            ConeTracing,
            _End/* keep last */
        };

        decltype(gPolygonModes)::const_iterator mPolygonMode = gPolygonModes.begin() + 2;

        ToneMapping mToneMapping = ToneMapping::AcesApprox;

        ShadowMethod mFinalSceneShadow = ShadowMethod::ShadowMap;

        // Aperture means full angle (2 * angle to the axis)
        math::Radian<GLfloat> mDiffuseConeAperture = math::Degree<GLfloat>{60.f};
        math::Radian<GLfloat> mShadowConeAperture = math::Degree<GLfloat>{10.f};
        GLfloat mSpecularConeRoughnessFactor = 1.0f;
        bool mGridAlign = false;

        float mDirectDiffuseFactor{1.0f};
        float mDirectSpecularFactor{1.0f};
        float mIndirectDiffuseFactor{1.0f};
        float mIndirectSpecularFactor{1.0f};

        math::Vec<2, GLfloat> mShadowScaleBias{1.f, 10.f};
    };

    FrameGraph(math::Size<2, int> aFrameSize);

    void resizeFrame(math::Size<2, int> aRenderResolution);

    void renderFinalScene(const scenic::SceneTree & aSceneTree,
                          Voxelizer & aVoxelizer);

    // TODO: It is unclear wether this is better to take a Voxelizer owning the voxel related resources
    // or that this would own all resources, and the voxelizer would have a reference to the FrameGraph
    // (second might be better so voxelizer could access other resources)
    void renderConeTrace(const scenic::SceneTree & aSceneTree,
                         Voxelizer & aVoxelizer,
                         GLuint aMode);

    enum class DepthMapType
    {
        TwoD,
        CubeMap,
    };
    void renderDepth(const scenic::SceneTree & aSceneTree, DepthMapType aType);

    void renderCubemap(const scenic::SceneTree & aSceneTree);

    void passForward(const scenic::SceneTree & aSceneTree,
                     const renderer::IntrospectProgram & aProgram);

    void loadPrograms();

    void appendUi();

    struct ProgramStore
    {
        ProgramStore(Engine & aEngine);

        renderer::IntrospectProgram mPbr;
        renderer::IntrospectProgram mConeTrace;
        renderer::IntrospectProgram mRayTraceVoxels;
        renderer::IntrospectProgram mDebugCubemap;
        renderer::IntrospectProgram mDepthMapping;
        renderer::IntrospectProgram mCubeDepthMapping;

        renderer::IntrospectProgram mVoxelizationProgram;
        renderer::IntrospectProgram mVoxelizationDominantAxisProgram;
        renderer::IntrospectProgram mVoxelizationViewProgram;
        renderer::IntrospectProgram mVoxelizationDominantAxisViewProgram;

        renderer::IntrospectProgram mInjectIrradianceProgram;
        renderer::IntrospectProgram mFilterIrradianceProgram;
    };


    Engine mEngine;
    ProgramStore mPrograms;
    graphics::VertexArrayObject mDummyVao;

    graphics::Texture mShadowMap;
    graphics::Texture mOmniShadowMap;
    graphics::FrameBuffer mShadowFramebuffer;
    graphics::UniformBufferObject mLightViewProjectionUbo;

    FrameControl mFrameControl;

};

} // namespace ad