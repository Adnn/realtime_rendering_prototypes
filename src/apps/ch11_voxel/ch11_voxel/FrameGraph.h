#pragma once


#include "Engine.h"

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

    struct FrameControl
    {
        inline static constexpr std::array<GLenum, 3> gPolygonModes{
             GL_POINT,
             GL_LINE,
             GL_FILL,
         }; 

        decltype(gPolygonModes)::const_iterator mPolygonMode = gPolygonModes.begin() + 2;
        math::Radian<GLfloat> mConeAperture = math::Degree<GLfloat>{30.f};
        bool mGridAlign = false;

        float mDirectDiffuseFactor{1.0f};
        float mDirectSpecularFactor{1.0f};
        float mIndirectDiffuseFactor{1.0f};
        float mIndirectSpecularFactor{1.0f};
    };

    FrameGraph(math::Size<2, int> aFrameSize);

    void resizeFrame(math::Size<2, int> aRenderResolution);

    void renderSimple(const scenic::SceneTree & aSceneTree,
                      Voxelizer & aVoxelizer);

    // TODO: It is unclear wether this is better to take a Voxelizer owning the voxel related resources
    // or that this would own all resources, and the voxelizer would have a reference to the FrameGraph
    // (second might be better so voxelizer could access other resources)
    void renderConeTrace(const scenic::SceneTree & aSceneTree,
                         Voxelizer & aVoxelizer,
                         GLuint aMode);

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

        renderer::IntrospectProgram mVoxelizationProgram;
        renderer::IntrospectProgram mVoxelizationDominantAxisProgram;
        renderer::IntrospectProgram mVoxelizationViewProgram;
        renderer::IntrospectProgram mVoxelizationDominantAxisViewProgram;

        renderer::IntrospectProgram mInjectIrradianceProgram;
    };


    Engine mEngine;
    ProgramStore mPrograms;
    graphics::VertexArrayObject mDummyVao;

    FrameControl mFrameControl;

};

} // namespace ad