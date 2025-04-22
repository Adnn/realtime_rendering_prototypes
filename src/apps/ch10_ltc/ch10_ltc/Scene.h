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
        .mPlanarCount = 1,
        // We decode a sRGB 10% white (which is also perceptually ~10%)
        // to linear space for computation.
        .mAmbientColor = math::decode_sRGB(math::hdr::gWhite<float> *0.1f),
        .mPlanarLights{
            CardLight_glsl{
                .mHeight = 2.f,
                .mRect{
                    .mPosition{0.f, 0.f},
                    .mDimension{2.f, 2.f},
                },
            }
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
