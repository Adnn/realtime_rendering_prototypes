#pragma once


#include "CameraSystem.h"
#include "Engine.h"
#include "Material.h"
#include "PlanarLights.h"

#include <engine/Entities.h>
#include <engine/IntrospectProgram.h>

#include <graphics/Timer.h>

#include <math/Color.h>
#include <math/Vector.h>

#include <renderer/UniformBuffer.h>
#include <renderer/VertexSpecification.h>
#include <renderer/Drawing.h>

#include <scenic/ColorPalettes.h>
#include <scenic/Shapes.h>


namespace ad {


constexpr math::hdr::Rgb<GLfloat> gBrickAlbedo{ 0.262f, 0.095f, 0.061f };

constexpr float gLightPowerScale = 8;
constexpr float gPlaneRotation = math::Degree<GLfloat>(30.f).as<math::Radian>().value();

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

    struct FrameControl
    {
        enum class AppMode
        {
            Shaded_scene,
            Ltc_viewer,
            Fig2,
            _End/*keep last*/
        };

        inline static constexpr std::array<GLenum, 3> gPolygonModes{
             GL_POINT,
             GL_LINE,
             GL_FILL,
         }; 

        AppMode mAppMode = AppMode::Shaded_scene;
        decltype(gPolygonModes)::const_iterator mPolygonMode = gPolygonModes.begin() + 2;
    };

    struct LtcControl
    {
        // Note: control alpha instead of roughness because the paper and plots are
        // using alpha as a dimension.
        float mAlpha = 0.3;
        math::Radian<float> mViewAngle{ math::Degree<float>{45.f} };
    };

    struct FigureControl
    {
        enum Letter : GLuint
        {
            a,
            b,
            c,
            d,
            _End/*keep last*/
        };
            
        Letter mLetter = c;
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

    graphics::VertexSpecification mSphereVertexSpecification;
    graphics::IndexBufferObject mSphereIndexBuffer;
    graphics::VertexSpecification mCardLightVertexSpecification;
    graphics::UniformBufferObject mEntitiesBlockBuffer;
    graphics::UniformBufferObject mViewProjectionBuffer;
    graphics::UniformBufferObject mMaterialsBlockBuffer;
    graphics::UniformBufferObject mLightsBlockBuffer;
    std::vector<renderer::IntrospectProgram> mSurfacePrograms;
    renderer::IntrospectProgram mLightProgram;
    graphics::Texture mLtcColorMap;
    graphics::Texture mLtc_1;
    graphics::Texture mLtc_2;

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
    PlanarLightsBlock mLights{
        .mPlanarCount = 5,
        // We decode a sRGB 10% white (which is also perceptually ~10%)
        // to linear space for computation.
        .mAmbientColor = math::decode_sRGB(math::hdr::gWhite<float> *0.1f),
        .mPlanarLights{
            CardLight_glsl{
                .mHeight = 3.5f,
                .mRect{
                    .mPosition{-2.f, -2.f},
                    .mDimension{4.f, 4.f},
                },
                .mColors = renderer::makeLightColors(
                    math::hdr::gWhite<GLfloat> *gLightPowerScale),
            },
            CardLight_glsl{
                .mHeight = -2.f + 2 * std::sin(gPlaneRotation),
                .mDoubleSided = true,
                .mRect{
                    .mPosition{-3.f + 2 * (1 - std::cos(gPlaneRotation)), -1.f},
                    .mDimension{2.f, 2.f},
                },
                .mRotationZ = -gPlaneRotation,
                .mColors = renderer::makeLightColors(
                    math::decode_sRGB(scenic::hdr::gNicePalette1_srgb[0]) * gLightPowerScale),
            },
            CardLight_glsl{
                .mHeight = -2.f,
                .mDoubleSided = true,
                .mRect{
                    .mPosition{1.f, -1.f},
                    .mDimension{2.f, 2.f},
                },
                .mRotationZ = gPlaneRotation,
                .mColors = renderer::makeLightColors(
                    math::decode_sRGB(scenic::hdr::gNicePalette1_srgb[1]) * gLightPowerScale),
            },
            CardLight_glsl{
                .mHeight = -2.f,
                .mDoubleSided = true,
                .mRect{
                    .mPosition{-1.f, 1.f},
                    .mDimension{2.f, 2.f},
                },
                .mRotationX = -gPlaneRotation,
                .mColors = renderer::makeLightColors(
                    math::decode_sRGB(scenic::hdr::gNicePalette1_srgb[2]) * gLightPowerScale),
            },
            CardLight_glsl{
                .mHeight = -2.f + 2 * std::sin(gPlaneRotation),
                .mDoubleSided = true,
                .mRect{
                    .mPosition{-1.f, -3.f + 2 * (1 - std::cos(gPlaneRotation))},
                    .mDimension{2.f, 2.f},
                },
                .mRotationX = gPlaneRotation,
                .mColors = renderer::makeLightColors(
                    math::decode_sRGB(scenic::hdr::gNicePalette1_srgb[3]) * gLightPowerScale),
            },
        }
    };
    OrbitalCamera mOrbitalCamera;

    TessellationControl mTessControl;
    FrameControl mFrameControl;
    LtcControl mLtcControl;
    FigureControl mFigureControl;
};


std::string to_string(Scene::FrameControl::AppMode aValue);

std::string to_string(Scene::FigureControl::Letter aValue);


} // namespace ad
