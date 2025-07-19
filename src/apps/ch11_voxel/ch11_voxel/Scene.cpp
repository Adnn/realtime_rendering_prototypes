#include "Scene.h"

#include "log/Logging.h"

#include "DebugDrawing.h"
#include "SetupDrawing.h"

#include <graphics/AppInterface.h>
#include <graphics/ApplicationGlfw.h>
#include <graphics/CameraUtilities.h>

#include <reflect/DearImguiWitness.h>
#include <reflect/ReflectHelpers.h>

#include <renderer/BufferIndexedBinding.h>
#include <renderer/BufferLoad.h>
#include <renderer/Uniforms.h>

#include <scenic/CameraGui.h>
#include <scenic/ColorPalettes.h>
#include <scenic/LoadScene.h>

#include <scenic/files/Archives.h>
#include <scenic/files/Serializer.h>

#include <ui/ImguiUi.h>
#include <ui/Widgets.h>
#include <ui/Widgets-impl.h>

#include <fmt/std.h>


namespace ad {

// TODO: make user controlled
constexpr unsigned int gGridDimension = 256;

void loadToBuffer(const renderer::EntitiesBlock_glsl & aData,
                  const graphics::UniformBufferObject & aBuffer,
                  graphics::BufferHint aUsageHint)
{
    graphics::load(aBuffer, std::span{ aData.mEntities }, aUsageHint);
}


//const renderer::ReferencePath gModelPaths[] = {renderer::ReferencePath{"models/Mat/meetmat_2.glb"}};
//constexpr float gModelScale = 0.1f;

//const renderer::ReferencePath gModelPaths[] = {renderer::ReferencePath{"models/Sponza/sponza.obj"}};
//constexpr float gModelScale = 0.01f;

//const renderer::ReferencePath gModelPaths[] = {renderer::ReferencePath{"models/Sponza-gltf/glTF/Sponza.gltf"}};
//constexpr float gModelScale = 1.f;

const renderer::ReferencePath gModelPaths[] = {renderer::ReferencePath{"models/pica-pica-mini-diorama-01/sketchfab_gltf/scene.gltf"}};
constexpr float gModelScale = 10.f;



//const renderer::ReferencePath gModelPaths[] = {
//    renderer::ReferencePath{"models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/tail-2.stl"},
//};
//constexpr float gModelScale = 0.01f;

//const renderer::ReferencePath gModelPaths[] = {renderer::ReferencePath{"models/4x4_cube/4x4_cube.gltf"}};
//constexpr float gModelScale = 1.f;

const std::filesystem::path gLightProgramPath = "programs/RenderModel_PlainColor.prog";


std::filesystem::path getCacheModel(renderer::ReferencePath aModel)
{
    aModel.mPath.replace_extension(".seum");
    return "se_cache" / aModel.mPath;
}


const bool gAssetCaching = false;

scenic::SceneTree prepareSceneTree(Engine & aEngine)
{
    scenic::SceneTree scene{
        .mTree = scenic::makeOneRootTree<scenic::Pose>()
    };

    for (const auto & reference : gModelPaths)
    {
        scenic::SceneTree modelScene;

        std::filesystem::path cacheCandidate = getCacheModel(reference);
        if (std::filesystem::is_regular_file(cacheCandidate) && gAssetCaching)
        {
            ADLOG(info)("Loading model from runtime archive for '{}'.", reference.mPath);
            scenic::Serializer serializer;
            scenic::FileInput archive{ cacheCandidate };
            serializer.serial(archive, modelScene);
        }
        else
        {
            ADLOG(warn)("Runtime archive absent for '{}', loading and serializing.", reference.mPath);
            auto fullPath = aEngine.mLoader.mFinder.pathFor(reference.mPath);
            scenic::loadModel(modelScene,
                              fullPath,
                              aEngine.mContext,
                              gModelScale);

            if (gAssetCaching)
            {
                std::filesystem::create_directories(cacheCandidate.parent_path());
                scenic::FileOutput archive{cacheCandidate};
                scenic::Serializer serializer;
                serializer.serial(archive, modelScene);
            }
        }
        scenic::mergeScenes(scene, modelScene, scene.mTree.mFirstRoot);
    }

    return scene;
}


// TODO: on framebuffer resize, inform the framegraph
Scene::Scene(graphics::AppInterface & aAppInterface, const imguiui::ImguiUi & aImgui) :
    mSizeListener{aAppInterface.listenFramebufferResize(
        std::bind(&Scene::onFramebufferResize, this, std::placeholders::_1))},
    mLightProgram{ mGraph.mEngine.loadProgram(renderer::ReferencePath{gLightProgramPath}) },
    mGraph(aAppInterface.getFramebufferSize()),
    mSceneTree{ prepareSceneTree(mGraph.mEngine) },
    mMaterials{mGraph.mEngine.mContext.mStorage.mMaterials}
    //, mEnvironment{ scenic::prepareEnvironment(gEnvMapPath, mGraph.mEngine.mLoader) }
{
    mOrbitalCamera.reset(math::getRatio<GLfloat>(aAppInterface.getWindowSize()));

    // Register the camera system with glfw inputs 
    graphics::registerGlfwCallbacks(
        aAppInterface,
        mOrbitalCamera.mOrbitalControl,
        graphics::EscKeyBehaviour::Close,
        // TODO: this is a dirty capture of a parameter given by reference
        &aImgui);

    // TODO use defines here for binding points
    graphics::bind(mViewProjectionBuffer, graphics::BindingIndex{ 0 });
    glObjectLabel(GL_BUFFER, mViewProjectionBuffer, -1, "ViewProjection");
    graphics::bind(mEntitiesBlockBuffer, graphics::BindingIndex{ 1 });
    glObjectLabel(GL_BUFFER, mEntitiesBlockBuffer, -1, "Entities");
    graphics::bind(mMaterialsBlockBuffer, graphics::BindingIndex{ 2 });
    glObjectLabel(GL_BUFFER, mMaterialsBlockBuffer, -1, "Materials");
    graphics::bind(mLightsBlockBuffer, graphics::BindingIndex{ 4 });
    glObjectLabel(GL_BUFFER, mLightsBlockBuffer, -1, "Lights");
}


void Scene::onFramebufferResize(math::Size<2, int> aNewSize)
{
    mGraph.resizeFrame(aNewSize);
}


void Scene::loadPrograms()
{
    mGraph.loadPrograms();
    mLightProgram =
        mGraph.mEngine.loadProgram(renderer::ReferencePath{ gLightProgramPath });
}


// TODO: move to a generic header
renderer::LightsDataCommon transformLightsData(
    renderer::LightsDataCommon aLightsData, // by value, as we need a copy
    const math::AffineMatrix<4, float> & aTransform)
{
    for (auto idx = 0; idx != aLightsData.mDirectionalCount; ++idx)
    {
        renderer::DirectionalLight_glsl & light = aLightsData.mDirectionalLights[idx];
        // might be unecessary to re-normalize, unless the transform scales
        aLightsData.mDirections_view[idx] = math::Vec<4, GLfloat>{
                math::UnitVec<3, GLfloat>{light.mDirection * aTransform.getLinear()},
                0.0f
        };
    }
    for (auto idx = 0; idx != aLightsData.mPointCount; ++idx)
    {
        renderer::PointLight_glsl & light = aLightsData.mPointLights[idx];
        aLightsData.mPoints_view[idx] = math::Position<4, GLfloat>{
            math::homogeneous::homogenize(math::homogeneous::makePosition(light.mPosition) * aTransform)
        };
    }

    return aLightsData;
}


void Scene::voxelize()
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "voxelization");

    // Note: should be called directly in the voxelize() function(s)
    mVoxelizer.recordSceneAabb(mSceneTree, gGridDimension);

    // We could be injecting the irradiance directly in the voxelization pass or a separate compute pass
    mVoxelizer.prepareIrradianceTexture(gGridDimension);

    mVoxelizer.mControl.mCpuReadVoxels = mSceneControl.mCubeInstances;

    if (mVoxelizer.mControl.mUseDominantAxis)
    {
        mVoxelizer.voxelizeDominantAxis(mSceneTree, gGridDimension, mGraph);
    }
    else
    {
        mVoxelizer.voxelize(mSceneTree, gGridDimension, mGraph);
    }

    // This is actually required to guarantee all writes are visible to subsequent
    // shader reads
    // TODO: place this barrier more tightly
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    if (mVoxelizer.mControl.mSeparateLightInjectionPass)
    {
        mVoxelizer.injectIrradianceComputePass(gGridDimension, mGraph);
    }
    else
    {
        mVoxelizer.fixupIrradianceAlphaComputePass(gGridDimension, mGraph);
    }
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    mVoxelizer.prepareMipmap(gGridDimension, mGraph);

    glPopDebugGroup();
}


void Scene::step(const graphics::Timer & aTimer,
                 math::Size<2, int> aWindowResolution)
{
    mOrbitalCamera.update(aTimer.delta(), aWindowResolution.height());

    //
    // Camera
    //
    changeAspectRatio(mOrbitalCamera.mCamera, math::getRatio<GLfloat>(aWindowResolution));
    graphics::loadSingle(mViewProjectionBuffer,
                         mOrbitalCamera.getViewProjectionBlock(),
                         graphics::BufferHint::StreamDraw);

    //
    // Materials
    // 
    graphics::loadSingle(mMaterialsBlockBuffer, mMaterials, graphics::BufferHint::StreamDraw);

    //
    // Lights
    //
    auto lights_cam =
        transformLightsData(mLights, mOrbitalCamera.mCamera.getParentToCamera());
    graphics::loadSingle(mLightsBlockBuffer, lights_cam, graphics::BufferHint::StreamDraw);

    //
    // Entities
    // 
    mObjectsCount = mSceneTree.mObjectsMap.size();
    // Ensure the vector can fit all objects and point lights
    mEntities.mEntities.resize(mObjectsCount + mLights.mPointCount);

    std::size_t objectIdx = 0;
    for (const auto & [nodeIdx, object] : mSceneTree.mObjectsMap)
    {
        auto & entity = mEntities.mEntities[objectIdx];
        entity.mLocalToWorld = static_cast<math::AffineMatrix<4, GLfloat>>(
            mSceneTree.mTree.mGlobalPose[nodeIdx]);
        entity.mColorFactor = math::hdr::gWhite<float>;
        ++objectIdx;
    }
    // Must be loaded before voxelization
    loadToBuffer(mEntities, mEntitiesBlockBuffer, graphics::BufferHint::StreamDraw);

    //
    // Shadow maps
    // 

    // TODO: limit to when light change
    mShadow.renderShadowMaps(mSceneTree, mLights, mGraph);


    if(mVoxelizationRequest)
    {
        voxelize();
        mVoxelizationRequest = false;
    }

    if (mSceneControl.showOccupancy() && mSceneControl.mCubeInstances)
    {
        const math::Box<float> & sceneAabb = mVoxelizer.mSceneAabb;

        mObjectsCount = std::pow(gGridDimension, 3);
        std::uint8_t * buffer =
            (std::uint8_t *)glMapNamedBufferRange(mVoxelizer.mVoxelStore,
                                                  offsetof(VoxelsSsbo_glsl, mVoxels),
                                                  mVoxelizer.mVoxelsByteSize,
                                                  GL_MAP_READ_BIT);

        //std::cerr << "From " << fragmentInvocations << " FS invocations: ";
        //for (unsigned int i = 0; i != mVoxelizer.mVoxelsByteSize; ++i)
        //{
        //    std::cerr << (unsigned)buffer[i] << " ";
        //}
        //std::cerr << std::endl;

        // Ensure the vector can fit all objects and point lights
        mEntities.mEntities.resize(mObjectsCount + mLights.mPointCount);

        float voxelSize = mVoxelizer.mVoxelSize;
        const auto scaling = math::trans3d::scaleUniform(voxelSize / 2);
        math::Vec<3, float> stride{voxelSize, voxelSize, voxelSize};
        math::Vec<3, float> baseOffset =
            sceneAabb.mPosition.as<math::Vec>() + stride / 2.f;
        unsigned int voxelIdx = 0;
        unsigned int entityIdx = 0;

        for (unsigned int y = 0; y != gGridDimension; ++y)
        {
            for (unsigned int x = 0; x != gGridDimension; ++x)
            {
                for (unsigned int z = 0; z != gGridDimension; ++z)
                {
                    if (buffer[voxelIdx] == 1)
                    {
                        auto & entity = mEntities.mEntities[entityIdx];
                        entity.mLocalToWorld =
                            scaling
                            * math::trans3d::translate(
                                baseOffset
                                + stride.cwMul({(float)x, (float)y, float(z)}));
                        entity.mColorFactor = math::hdr::gWhite<float>;
                        ++entityIdx;
                    }
                    ++voxelIdx;
                }
            }
        }
        glUnmapNamedBuffer(mVoxelizer.mVoxelStore);

        // tighten the object count to just include populated entities
        mObjectsCount = entityIdx;
    }

    for (std::size_t lightIdx = 0; lightIdx != mLights.mPointCount; ++lightIdx)
    {
        const auto & light = mLights.mPointLights[lightIdx];
        auto & entity = mEntities.mEntities[mObjectsCount + lightIdx];
        entity.mLocalToWorld =
            math::trans3d::scaleUniform(light.mRadius.mMin)
            * math::trans3d::translate(light.mPosition.as<math::Vec>());
        entity.mColorFactor = light.mColors.mSpecularColor;
    }
    // TODO: address this duplicate load of the entities buffer
    loadToBuffer(mEntities, mEntitiesBlockBuffer, graphics::BufferHint::StreamDraw);
}


void Scene::render(math::Size<2, int> aBackbufferResolution)
{
    renderTo(graphics::FrameBuffer::Default(), aBackbufferResolution);
}


void Scene::renderTo(const graphics::FrameBuffer & aFramebuffer, math::Size<2, int> aBackbufferResolution)
{

    //
    // Frame rendering
    //
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, aFramebuffer);
    glViewport(0, 0, aBackbufferResolution.width(), aBackbufferResolution.height());
    glClearColor(0.1f, 0.2f, 0.3f, 1.f);
    // Required to actually clear the depth buffer
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mSceneControl.showVoxels())
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        if (mSceneControl.showOccupancy() && mSceneControl.mCubeInstances)
        {
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "render_voxels_mesh");

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);

            const auto & program = mGraph.mPrograms.mPbr;
            glUseProgram(program);

            for (const scenic::MeshPart_Naive & part : mCube.mParts)
            {
                graphics::VertexArrayObject vao = prepareVAO(program, part);
                glBindVertexArray(vao);

                if (scenic::useElementIndices(part))
                {
                    glDrawElementsInstancedBaseInstance(
                        part.mPrimitiveMode,
                        part.mIndicesCount,
                        part.mIndicesType,
                        (void *)part.mIndexFirst,
                        mObjectsCount, /* instances count */
                        0  /* base instance, voxel instances are first in the UBO */);
                }
                else
                {
                    throw std::logic_error{"Who is not using indexed rendering?"};
                }
            }

            glPopDebugGroup();
        }
        else
        {
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "render_voxels_raytrace");

            glBindVertexArray(mGraph.mDummyVao);

            const auto & program = mGraph.mPrograms.mRayTraceVoxels;
            glUseProgram(program);

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, mVoxelizer.mVoxelStore);

            const math::Box<float> & aabb = mVoxelizer.mSceneAabb;
            graphics::setUniform(program, "u_AabbMin", aabb.leftBottomZMin());
            graphics::setUniform(program, "u_AabbMax", aabb.rightTopZMax());

            graphics::setUniform(program, "u_FramebufferSize", aBackbufferResolution);
            
            const graphics::PerspectiveParameters projectionParams =
                std::get<graphics::PerspectiveParameters>
                (mOrbitalCamera.mCamera.getProjectionParameters());
            float imageHeight = 2 * tan(projectionParams.mVerticalFov / 2);
            math::Size<2, float> imagePlaneSize{
                projectionParams.mAspectRatio * imageHeight,
                imageHeight
            };
            graphics::setUniform(program, "u_ImagePlane_view", imagePlaneSize);

            graphics::setUniform(program, "u_VoxelSize", mVoxelizer.mVoxelSize);

            glBindTextureUnit(0, mVoxelizer.mAlbedo);
            graphics::setUniform(program, "u_VoxelsAlbedoTexture", 0);
            glBindTextureUnit(1, mVoxelizer.mNormals);
            graphics::setUniform(program, "u_VoxelsNormalsTexture", 1);
            glBindTextureUnit(2, mVoxelizer.mIrradiance);
            graphics::setUniform(program, "u_VoxelsIrradianceTexture", 2);

            graphics::setUniform(program, "u_VoxelMode",
                                 static_cast<GLuint>(mSceneControl.mMode));
            graphics::setUniform(program, "u_VoxelMipmapLevel",
                                 mSceneControl.mMipmapLevel);

            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glPopDebugGroup();
        }
        
    }
    else if (mSceneControl.mVoxelPov)
    {
        int min = *aBackbufferResolution.getMinMagnitudeElement();
        glViewport(0, 0, min, min);
        if (mVoxelizer.mControl.mUseDominantAxis)
        {
            mVoxelizer.voxelizeDominantAxisView(mSceneTree, gGridDimension, mGraph);
        }
        else
        {
            mVoxelizer.voxelizeView(mSceneTree, gGridDimension, mGraph);
        }
    }
    else if (mSceneControl.showConeTrace())
    {
        mGraph.renderConeTrace(mSceneTree, mVoxelizer, (GLuint)mSceneControl.mMode);
    }
    else
    {
        mGraph.renderFinalScene(mSceneTree, mVoxelizer);
        //mGraph.renderCubemap(mSceneTree);
    }

    //
    // Draw lights
    //
    if (mSceneControl.mShowPunctualLights)
    {
        glUseProgram(mLightProgram);

        for (const scenic::MeshPart_Naive & part : mSphere.mParts)
        {
            graphics::VertexArrayObject vao = prepareVAO(mLightProgram, part);
            glBindVertexArray(vao);

            if (scenic::useElementIndices(part))
            {
                glDrawElementsInstancedBaseInstance(
                    part.mPrimitiveMode,
                    part.mIndicesCount,
                    part.mIndicesType,
                    (void *)part.mIndexFirst,
                    mLights.mPointCount, /* instances count */
                    mObjectsCount /* base instance, light instances are after objects in UBOs */);
            }
            else
            {
                throw std::logic_error{"Who is not using indexed rendering?"};
            }
        }
    }

    DBGDRAW.startFrame();
    if (mSceneControl.mDrawBoundingBoxes)
    {
        for (const auto & [nodeIdx, object] : mSceneTree.mObjectsMap)
        {
            DBGDRAW.addBox(
                object.mAabb,
                mSceneTree.mTree.mGlobalPose[nodeIdx],
                math::hdr::gGreen<float>);
        }
        DBGDRAW.addBox(scenic::getAabb(mSceneTree),
                       scenic::Pose{},
                       math::hdr::gCyan<float>);
    }
    DBGDRAW.endFrame();
    mDebugRenderer.render(DBGDRAW.mFrameCommands);
}


void Scene::presentUi(bool * aOpen)
{
    ImGui::Begin("Scene", aOpen);

    if (ImGui::Button("Recompile shaders"))
    {
        try
        {
            loadPrograms();
            // We have a separate button for that, but it is easy to get confused
            // when recompiling voxelization shaders if the voxelization is not re-applied
            mVoxelizationRequest = true;
        }
        catch (const std::exception & aException)
        {
            ADLOG(error)("Exception thrown while compiling technique:\n{}",
                         aException.what());
        }
    }

    // Scene control
    imguiui::addComboContinuousEnum<SceneControl::Mode::_End>(
        "Mode", mSceneControl.mMode);
    ImGui::Indent();
    {
        if (!mSceneControl.showOccupancy()) ImGui::BeginDisabled();
        ImGui::Checkbox("Instantiate meshes", &mSceneControl.mCubeInstances);
        if (!mSceneControl.showOccupancy()) ImGui::EndDisabled();
    }
    ImGui::Unindent();

    {
        GLint levelCount = 1;
        if (mSceneControl.showIrradiance())
        {
            glGetTextureParameteriv(mVoxelizer.mIrradiance, GL_TEXTURE_IMMUTABLE_LEVELS, &levelCount);
        }
        else
        {
            mSceneControl.mMipmapLevel = 0;
        }
        // see: https://github.com/ocornut/imgui/issues/3959#issuecomment-804105240
        ImGuiSliderFlags flags = (levelCount == 1) ? ImGuiSliderFlags_ClampZeroRange : 0;
        flags |= ImGuiSliderFlags_ClampOnInput;
        ImGui::DragInt("Mipmap level", &mSceneControl.mMipmapLevel, .1f, 0, levelCount - 1, "%d", flags);
    }

    ImGui::Checkbox("Show Punctual Lights", &mSceneControl.mShowPunctualLights);
    ImGui::Checkbox("Draw BB", &mSceneControl.mDrawBoundingBoxes);

    mVoxelizationRequest |= ImGui::Button("Force voxelize");
    if (ImGui::CollapsingHeader("Voxelization"))
    {
        mVoxelizationRequest |= ImGui::Checkbox("Dominant Axis Method", &mVoxelizer.mControl.mUseDominantAxis);
        mVoxelizationRequest |= ImGui::Checkbox("Separate compute light injection", &mVoxelizer.mControl.mSeparateLightInjectionPass);
        mVoxelizationRequest |= ImGui::Checkbox("Conservative Rasterization", &mVoxelizer.mControl.mConservativeRasterization);
        mVoxelizationRequest |= ImGui::Checkbox("Conservative Depth Range", &mVoxelizer.mControl.mConservativeDepthRange);
        mVoxelizationRequest |= ImGui::Checkbox("Average Samples in Voxel", &mVoxelizer.mControl.mAverageSamples);
        mVoxelizationRequest |= ImGui::Checkbox("Average Normals by axis", &mVoxelizer.mControl.mAverageNormalByAxis);
        mVoxelizationRequest |= ImGui::Checkbox("Trace linear filtering", &mVoxelizer.mControl.mLinearFiltering);
        mVoxelizationRequest |= ImGui::Checkbox("Compute Shader Irradiance Filtering", &mVoxelizer.mControl.mComputeIrradianceMipmapping);
        ImGui::Checkbox("Voxel POV", &mSceneControl.mVoxelPov);
    }

    DearImguiWitness witness;

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Frame Graph"))
    {
        mGraph.appendUi();
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Materials"))
    {
        describe(witness, mMaterials, mGraph.mEngine.mContext.mStorage.mMaterialNames);
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Lights"))
    {
        describe(witness, mLights);
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Camera"))
    {
        scenic::appendUi(mOrbitalCamera);
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Scene tree"))
    {
        scenic::presentNodeTree(mSceneTree.mTree, mSceneTree.mTree.mFirstRoot, mSceneTreeGuiState);
    }

    if (auto selected = mSceneTreeGuiState.mSelected;
        selected!= scenic::Node::gInvalidIndex)
    {
        std::string storage;
        const std::string & name = mSceneTree.mTree.getSafeName(selected, storage);
        ImGui::Begin(name.c_str(), aOpen);
        if (auto modified = scenic::presentPose(mSceneTree.mTree.mLocalPose[selected]))
        {
            mSceneTree.mTree.setLocalPose(selected, *modified);
        }
        ImGui::End();
    }

    ImGui::End();
}


std::string to_string(Scene::SceneControl::Mode aValue)
{
#define STR(enumerator) case Scene::SceneControl::Mode::enumerator: return #enumerator
    switch (aValue)
    {
        STR(FullScene);
        STR(ConeTrace_AO);
        STR(ConeTrace_Diffuse);
        STR(ConeTrace_Specular);
        STR(VoxelsOccupancy);
        STR(VoxelsAlbedo);
        STR(VoxelsNormals);
        STR(VoxelsIrradiance);
    default:
        throw std::logic_error{ "Unhandled mode." };
    }
#undef STR
}


} // namespace ad
