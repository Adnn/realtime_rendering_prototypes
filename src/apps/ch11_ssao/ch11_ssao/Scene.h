#pragma once


#include "Engine.h"
#include "FrameGraph.h"
#include "Material.h"

#include <engine/Entities.h>
#include <engine/IntrospectProgram.h>
#include <engine/Lights.h>

#include <graphics/AppInterface.h>
#include <graphics/Timer.h>

#include <math/Color.h>
#include <math/Vector.h>

#include <renderer/UniformBuffer.h>
#include <renderer/VertexSpecification.h>
#include <renderer/Drawing.h>

#include <scenic/CameraSystem.h>
#include <scenic/ColorPalettes.h>
#include <scenic/ShapesAsModel.h>

#include <scenic/environment/Environment.h>


namespace ad {



namespace imguiui {
    class ImguiUi;
} // namespace imguiui


struct Scene
{
    struct SceneControl
    {
        bool mShowPunctualLights = true;
        bool mShowTexture = false;
        TextureStore::Name mTexture = TextureStore::RawOcclusion;
    };

    Scene(graphics::AppInterface & aAppInterface, const imguiui::ImguiUi & aImgui);

    void onFramebufferResize(math::Size<2, int> aNewSize);

    void loadPrograms();

    void step(
        const graphics::Timer & aTimer,
        math::Size<2, int> aWindowResolution);

    void render(math::Size<2, int> aBackbufferResolution);
    void renderTo(const graphics::FrameBuffer & aFramebuffer, math::Size<2, int> aBackbufferResolution);

    void presentUi(bool * aOpen = nullptr);

    FrameGraph mGraph;
    std::shared_ptr<graphics::AppInterface::SizeListener> mSizeListener;

    graphics::UniformBufferObject mEntitiesBlockBuffer;
    graphics::UniformBufferObject mViewProjectionBuffer;
    graphics::UniformBufferObject mMaterialsBlockBuffer;
    graphics::UniformBufferObject mLightsBlockBuffer;
    renderer::IntrospectProgram mSurfaceProgram;
    renderer::IntrospectProgram mLightProgram;

    scenic::OrbitalCamera mOrbitalCamera;
    scenic::SceneTree mSceneTree;
    scenic::Object mSphere{ scenic::makeSphere(4) };
    scenic::Environment mEnvironment;

    renderer::EntitiesBlock_glsl mEntities;
    PbrMaterialsBlock_glsl mMaterials{
        .mCount = 1,
        .mMaterials = {
            PbrMaterial_glsl{
                .mBaseColor{scenic::hdr::gBrickAlbedo},
                //.mBaseColor{scenic::hdr::gGoldAlbedo},
                //.mMetallic = 1.f,
            },
        },
    };
    renderer::LightsDataCommon mLights{
        .mDirectionalCount = 0,
        .mPointCount = 1,
        // We decode a sRGB 50% white (which is also perceptually ~50%)
        // to linear space for computation.
        .mAmbientColor = math::decode_sRGB(math::hdr::gWhite<float> * 0.5f),
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
                .mPosition = {0.0f, 1.7f, 1.0f},
                .mRadius{
                    .mMin = 0.25f,
                    .mMax = 5.f,
                },
                .mColors = renderer::LightColors_glsl{} * 20.f,
            },
            renderer::PointLight_glsl{
                .mPosition = {+2.f, 3.f, 0.f},
                .mRadius{
                    .mMin = 0.2f,
                    .mMax = 5.f,
                },
                .mColors = renderer::LightColors_glsl{} * 20.f,
            },
         },
    };

    SceneControl mSceneControl;
};


} // namespace ad
