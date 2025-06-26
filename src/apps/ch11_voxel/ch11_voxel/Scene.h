#pragma once


#include "Engine.h"
#include "FrameGraph.h"
#include "Voxelization.h"

#include "debug/DebugRenderer.h"

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

#include <scenic/gui/HierarchyGui.h>


namespace ad {



namespace imguiui {
    class ImguiUi;
} // namespace imguiui


struct Scene
{
    struct SceneControl
    {
        enum class Mode 
        {
            FullScene,
            ConeTrace,
            VoxelsOccupancy,
            VoxelsAlbedo,
            VoxelsNormals,
            VoxelsIrradiance,
            _End/* keep last */
        };

        bool showOccupancy() const
        {
            return mMode == Mode::VoxelsOccupancy;
        };

        bool showIrradiance() const
        {
            return mMode == Mode::VoxelsIrradiance;
        };

        bool showVoxels() const
        {
            return 
                mMode == Mode::VoxelsOccupancy
                || mMode == Mode::VoxelsAlbedo
                || mMode == Mode::VoxelsNormals
                || mMode == Mode::VoxelsIrradiance
                ;
        };

        Mode mMode{ Mode::FullScene };
        GLint mMipmapLevel = 0;
        bool mCubeInstances = false;

        bool mShowPunctualLights = false;
        bool mDrawBoundingBoxes = true;
        bool mVoxelPov = false;
    };

    Scene(graphics::AppInterface & aAppInterface, const imguiui::ImguiUi & aImgui);

    void onFramebufferResize(math::Size<2, int> aNewSize);

    void loadPrograms();

    void voxelize();

    void step(
        const graphics::Timer & aTimer,
        math::Size<2, int> aWindowResolution);

    void render(math::Size<2, int> aBackbufferResolution);
    void renderTo(const graphics::FrameBuffer & aFramebuffer, math::Size<2, int> aBackbufferResolution);

    void presentUi(bool * aOpen = nullptr);

    // Must be initialized early, to populate Engine prefixes
    FrameGraph mGraph;

    scenic::SceneTree mSceneTree;
    scenic::GenericMaterialsBlock_glsl & mMaterials;
    scenic::OrbitalCamera mOrbitalCamera;
    //scenic::Environment mEnvironment;
    scenic::Object mSphere{ scenic::makeSphere(4) };
    scenic::Object mCube{ scenic::makeCube() };
    SceneControl mSceneControl;
    scenic::TreeInteractionState mSceneTreeGuiState;
    debug::DebugRenderer mDebugRenderer{mGraph.mEngine};
    bool mVoxelizationRequest = true;
    Voxelizer mVoxelizer;

    std::shared_ptr<graphics::AppInterface::SizeListener> mSizeListener;

    // TODO: should mostly move to framegraph
    graphics::UniformBufferObject mEntitiesBlockBuffer;
    graphics::UniformBufferObject mLightsBlockBuffer;
    graphics::UniformBufferObject mMaterialsBlockBuffer;
    graphics::UniformBufferObject mViewProjectionBuffer;
    renderer::IntrospectProgram mLightProgram;

    renderer::EntitiesBlock_glsl mEntities;
    // The count of entities to be rendered that are not lights
    // (i.e.: this is the index of the first light in the entities buffer)
    unsigned int mObjectsCount = 0;

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
                .mPosition = {0.0f, 4.0f, 0.0f},
                .mRadius{
                    .mMin = 0.5f,
                    .mMax = 30.f,
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
};


std::string to_string(Scene::SceneControl::Mode aValue);


} // namespace ad
