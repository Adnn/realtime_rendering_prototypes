#pragma once


#include "CameraSystem.h"
#include "Engine.h"
#include "Material.h"

#include <engine/Entities.h>
#include <engine/IntrospectProgram.h>

#include <graphics/Timer.h>

#include <math/Color.h>
#include <math/Vector.h>

#include <renderer/UniformBuffer.h>
#include <renderer/VertexSpecification.h>
#include <renderer/Drawing.h>

#include <scenic/ColorPalettes.h>


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


struct Scene
{
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

    void loadPrograms();

    void step(
        const graphics::Timer & aTimer,
        math::Size<2, int> aWindowResolution);

    void render(math::Size<2, int> aRenderResolution);

    void presentUi(bool * aOpen = nullptr);

    Engine mEngine;

    graphics::UniformBufferObject mEntitiesBlockBuffer;
    graphics::UniformBufferObject mViewProjectionBuffer;
    graphics::UniformBufferObject mMaterialsBlockBuffer;
    graphics::UniformBufferObject mLightsBlockBuffer;
    renderer::IntrospectProgram mSurfaceProgram;

    OrbitalCamera mOrbitalCamera;
    scenic::SceneTree mSceneTree;

    renderer::EntitiesBlock_glsl mEntities{
        .mEntities = {
            renderer::EntityData_glsl{
                .mLocalToWorld = math::AffineMatrix<4, GLfloat>::Identity(),
                .mColorFactor = math::hdr::gWhite<float>,
            },
        },
    };
    PbrMaterialsBlock_glsl mMaterials{
        .mCount = 1,
        .mMaterials = {
            PbrMaterial_glsl{
                .mBaseColor{scenic::hdr::gBrickAlbedo},
            },
        },
    };

    PipelineControl mPipelineControl;
};


} // namespace ad
