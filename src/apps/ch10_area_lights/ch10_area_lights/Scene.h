#pragma once


#include "CameraSystem.h"
#include "Engine.h"
#include "Material.h"

#include <engine/Entities.h>
#include <engine/IntrospectProgram.h>
#include <engine/Lights.h>

#include <graphics/Timer.h>

#include <math/Color.h>
#include <math/Vector.h>

#include <renderer/UniformBuffer.h>
#include <renderer/VertexSpecification.h>
#include <renderer/Drawing.h>

#include <scenic/Shapes.h>


namespace ad {


constexpr math::hdr::Rgb<GLfloat> gBrickAlbedo{ 0.262f, 0.095f, 0.061f };


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
    GLuint mEntityIdx;
};

constexpr graphics::AttributeDescriptionList gInstanceDescription{
    {   graphics::ShaderParameter{1, graphics::ShaderParameter::Access::Integer},
        1, offsetof(Instance, mEntityIdx), graphics::MappedGL<GLuint>::enumerator },
};


// For the moment, each instance maps to a distinct entity
// and we store all entity data in a UBO
//static std::array<Instance, 2> gInstances{
//    0,
//    1,
//};


struct Scene
{
    struct TessellationControl
    {
        TessellationControl()
        {
            glGetIntegerv(GL_MAX_PATCH_VERTICES, &mMaxPatchVertices);
            glGetIntegerv(GL_MAX_TESS_GEN_LEVEL, &mMaxTessGenLevel);
        }

        static constexpr GLfloat gLevel = 1;
        GLuint mPatchVertices = 3;
        math::Vec<4, GLfloat> mOuterLevel{ gLevel, gLevel, gLevel, 1.f };
        math::Vec<2, GLfloat> mInnerLevel{ gLevel, 1.f };

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

    void loadPrograms();

    void step(
        const graphics::Timer & aTimer,
        math::Size<2, int> aWindowResolution);

    void render(math::Size<2, int> aRenderResolution);

    void presentUi(bool * aOpen = nullptr);

    Engine mEngine;

    scenic::geodesic::Sphere mSphere{ 4 };
    GLsizei mIndicesCount{ (GLsizei)mSphere.mIndices.size() };

    graphics::VertexSpecification mVertexSpecification;
    graphics::IndexBufferObject mIndexBuffer;
    graphics::UniformBufferObject mEntitiesBlockBuffer;
    graphics::UniformBufferObject mViewProjectionBuffer;
    graphics::UniformBufferObject mMaterialsBlockBuffer;
    graphics::UniformBufferObject mLightsBlockBuffer;
    renderer::IntrospectProgram mSurfaceProgram;
    renderer::IntrospectProgram mLightProgram;

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
                .mBaseColor{gBrickAlbedo},
            },
        },
    };
    renderer::LightsDataCommon mLights{
        .mDirectionalCount = 0,
        .mPointCount = 2,
        // We decode a sRGB 10% white (which is also perceptually ~10%)
        // to linear space for computation.
        .mAmbientColor = math::decode_sRGB(math::hdr::gWhite<float> * 0.1f),
        .mDirectionalLights = {
            renderer::DirectionalLight_glsl{
                .mDirection = math::UnitVec<3, float>{ {0.5f, 0.f, -0.5f} },
                // TODO: decode the srgb value to have it show correctly in Imgui
                // (and have it perceptually proportional to the factor)
                .mColors = renderer::LightColors_glsl{} * 0.2,
            },
         },
        .mPointLights = {
            renderer::PointLight_glsl{
                .mPosition = {-1.f, 3.f, 0.f},
                .mRadius{
                    .mMin = 1.f,
                    .mMax = 5.f,
                },
                .mColors = renderer::LightColors_glsl{} * 2.f,
            },
            renderer::PointLight_glsl{
                .mPosition = {+1.f, 3.f, 0.f},
                .mRadius{
                    .mMin = 1.f,
                    .mMax = 5.f,
                },
                .mColors = renderer::LightColors_glsl{} * 2.f,
            },
         },
    };
    OrbitalCamera mOrbitalCamera;

    TessellationControl mTessControl;
    PipelineControl mPipelineControl;
};


} // namespace ad
