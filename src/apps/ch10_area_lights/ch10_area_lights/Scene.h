#pragma once


#include "CameraSystem.h"
#include "Engine.h"

#include <engine/IntrospectProgram.h>

#include <graphics/Timer.h>

#include <math/Color.h>
#include <math/Vector.h>

#include <renderer/UniformBuffer.h>
#include <renderer/VertexSpecification.h>
#include <renderer/Drawing.h>

#include <scenic/Shapes.h>


namespace ad {


namespace graphics {
    class AppInterface;
} // namespace graphics


namespace imguiui {
    class ImguiUi;
} // namespace imguiui


constexpr graphics::AttributeDescriptionList gVertexDescription{
    {0, 3, /*offset*/0, graphics::MappedGL<GLfloat>::enumerator},
};


struct Instance
{
    math::hdr::Rgb_f mColor = math::hdr::gGreen<float>;
};

constexpr graphics::AttributeDescriptionList gInstanceDescription{
    {1, 3, offsetof(Instance, mColor), graphics::MappedGL<GLfloat>::enumerator},
};


static std::array<Instance, 1> gInstances{};


struct Scene
{
    struct TessellationControl
    {
        TessellationControl()
        {
            glGetIntegerv(GL_MAX_PATCH_VERTICES, &mMaxPatchVertices);
            glGetIntegerv(GL_MAX_TESS_GEN_LEVEL, &mMaxTessGenLevel);
        }

        GLuint mPatchVertices = 3;
        math::Vec<4, GLfloat> mOuterLevel{ 2.f, 2.f, 2.f, 1.f };
        math::Vec<2, GLfloat> mInnerLevel{ 2.f, 1.f };

        GLint mMaxPatchVertices;
        GLint mMaxTessGenLevel;
    };

    struct PipelineControl
    {
       inline static constexpr std::array<GLenum, 3> gPolygonModes{
            GL_POINT,
            GL_LINE,
            GL_FILL,
        }; 

       decltype(gPolygonModes)::const_iterator mPolygonMode = gPolygonModes.begin() + 2;
    };

    Scene(graphics::AppInterface & aAppInterface, const imguiui::ImguiUi & aImgui);

    void step(
        const graphics::Timer & aTimer,
        math::Size<2, int> aWindowResolution);

    void render(math::Size<2, int> aRenderResolution);

    void presentUi(bool * aOpen = nullptr);

    Engine mEngine;

    graphics::VertexSpecification mVertexSpecification;
    graphics::IndexBufferObject mIndexBuffer;
    graphics::UniformBufferObject mViewProjectionBuffer;
    graphics::UniformBufferObject mLightsBlockBuffer;
    renderer::IntrospectProgram mIntrospectProgram;

    OrbitalCamera mOrbitalCamera;

    TessellationControl mTessControl;
    PipelineControl mPipelineControl;
};


} // namespace ad
