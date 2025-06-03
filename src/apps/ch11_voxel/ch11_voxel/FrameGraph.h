#pragma once


#include "Engine.h"

#include <renderer/FrameBuffer.h>
#include <renderer/Shading.h>
#include <renderer/Texture.h>

#include <scenic/Model.h>

#include <scenic/environment/Environment.h>


namespace ad {

void drawPass(const renderer::IntrospectProgram & aProgram,
              const scenic::SceneTree& aSceneTree);



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
    };

    FrameGraph(math::Size<2, int> aFrameSize);

    void resizeFrame(math::Size<2, int> aRenderResolution);

    void renderSimple(const scenic::SceneTree & aSceneTree);

    void passBlinnPhong(const scenic::SceneTree & aSceneTree);

    void loadPrograms();

    void appendUi();

    struct ProgramStore
    {
        ProgramStore(Engine & aEngine);

        renderer::IntrospectProgram mBlinnPhong;
        renderer::IntrospectProgram mRayTraceVoxels;

        renderer::IntrospectProgram mVoxelizationProgram;
        renderer::IntrospectProgram mVoxelizationDominantAxisProgram;
        renderer::IntrospectProgram mVoxelizationViewProgram;
        renderer::IntrospectProgram mVoxelizationDominantAxisViewProgram;
    };


    Engine mEngine;
    ProgramStore mPrograms;
    graphics::VertexArrayObject mDummyVao;

    FrameControl mFrameControl;

};

} // namespace ad