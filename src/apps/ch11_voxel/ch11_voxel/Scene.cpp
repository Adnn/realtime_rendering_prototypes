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

constexpr unsigned int gGridDimension = 512;

void loadToBuffer(const renderer::EntitiesBlock_glsl & aData,
                  const graphics::UniformBufferObject & aBuffer,
                  graphics::BufferHint aUsageHint)
{
    graphics::load(aBuffer, std::span{ aData.mEntities }, aUsageHint);
}


//const renderer::ReferencePath gModelPaths[] = {renderer::ReferencePath{"models/Mat/meetmat_2.glb"}};
//constexpr float gModelScale = 0.1f;

const renderer::ReferencePath gModelPaths[] = {
    renderer::ReferencePath{"models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/tail-2.stl"},
};
constexpr float gModelScale = 0.01f;

//const renderer::ReferencePath gModelPaths[] = {renderer::ReferencePath{"models/4x4_cube/4x4_cube.gltf"}};
//constexpr float gModelScale = 1.f;

const std::filesystem::path gLightProgramPath = "programs/RenderModel_PlainColor.prog";


std::filesystem::path getCacheModel(renderer::ReferencePath aModel)
{
    aModel.mPath.replace_extension(".seum");
    return "se_cache" / aModel.mPath;
}


scenic::SceneTree prepareSceneTree(Engine & aEngine)
{
    scenic::SceneTree scene{
        .mTree = scenic::makeOneRootTree<scenic::Pose>()
    };

    for (const auto & reference : gModelPaths)
    {
        scenic::SceneTree modelScene;

        std::filesystem::path cacheCandidate = getCacheModel(reference);
        if (std::filesystem::is_regular_file(cacheCandidate))
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

            std::filesystem::create_directories(cacheCandidate.parent_path());
            scenic::FileOutput archive{ cacheCandidate };
            scenic::Serializer serializer;
            serializer.serial(archive, modelScene);
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
    mSceneTree{ prepareSceneTree(mGraph.mEngine) }
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
        light.mDirection = math::UnitVec<3, GLfloat>{
            light.mDirection * aTransform.getLinear() };
    }
    for (auto idx = 0; idx != aLightsData.mPointCount; ++idx)
    {
        renderer::PointLight_glsl & light = aLightsData.mPointLights[idx];
        light.mPosition = math::homogeneous::homogenize(
            math::homogeneous::makePosition(light.mPosition) * aTransform).xyz();
    }

    return aLightsData;
}


void Scene::voxelize()
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "voxelization");

    const math::Box<float> sceneAabb = scenic::getAabb(mSceneTree);
    const float maxSide = *sceneAabb.mDimension.getMaxMagnitudeElement();
    mVoxelSize = maxSide / gGridDimension;

    mVoxelizer.mControl.mCpuReadVoxels = !mSceneControl.mRaytraceVoxels;

    if (mVoxelizer.mControl.mUseDominantAxis)
    {
        mVoxelizer.voxelizeDominantAxis(mSceneTree, gGridDimension, mViewProjectionBuffer, mGraph);
    }
    else
    {
        mVoxelizer.voxelize(mSceneTree, gGridDimension, mViewProjectionBuffer, mGraph);
    }

    //mVoxelizer.prepareMipmap(gGridDimension);

    // This is actually required to guarantee all writes are visible to subsequent
    // shader reads
    // TODO: place this barrier more tightly
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glPopDebugGroup();
}


void Scene::step(const graphics::Timer & /*aTimer*/,
                 math::Size<2, int> aWindowResolution)
{
    mOrbitalCamera.update(aWindowResolution.height());

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


    if(mVoxelizationRequest)
    {
        voxelize();
        mVoxelizationRequest = false;
    }

    if (mSceneControl.mShowVoxels && !mSceneControl.mRaytraceVoxels)
    {
        const math::Box<float> sceneAabb = scenic::getAabb(mSceneTree);

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

        const auto scaling = math::trans3d::scaleUniform(mVoxelSize / 2);
        math::Vec<3, float> stride{mVoxelSize, mVoxelSize, mVoxelSize};
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
    // Camera
    //
    changeAspectRatio(mOrbitalCamera.mCamera, math::getRatio<GLfloat>(aBackbufferResolution));
    graphics::loadSingle(mViewProjectionBuffer,
                         mOrbitalCamera.getViewProjectionBlock(),
                         graphics::BufferHint::StreamDraw);

    //
    // Frame rendering
    //
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, aFramebuffer);
    glViewport(0, 0, aBackbufferResolution.width(), aBackbufferResolution.height());
    glClearColor(0.1f, 0.2f, 0.3f, 1.f);
    // Required to actually clear the depth buffer
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mSceneControl.mShowVoxels)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        if (mSceneControl.mRaytraceVoxels)
        {
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "render_voxels_raytrace");

            glBindVertexArray(mGraph.mDummyVao);

            const auto & program = mGraph.mPrograms.mRayTraceVoxels;
            glUseProgram(program);

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, mVoxelizer.mVoxelStore);

            // TODO cache the aabb
            math::Box<float> aabb = scenic::getAabb(mSceneTree);
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

            graphics::setUniform(program, "u_VoxelSize", mVoxelSize);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glPopDebugGroup();
        }
        else
        {
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "render_voxels_mesh");

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);

            const auto & program = mGraph.mPrograms.mBlinnPhong;
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
    }
    else
    {
        if (mSceneControl.mVoxelPov)
        {
            int min = *aBackbufferResolution.getMinMagnitudeElement();
            glViewport(0, 0, min, min);
            if (mVoxelizer.mControl.mUseDominantAxis)
            {
                mVoxelizer.voxelizeDominantAxisView(mSceneTree, gGridDimension, mViewProjectionBuffer, mGraph);
            }
            else
            {
                mVoxelizer.voxelizeView(mSceneTree, gGridDimension, mViewProjectionBuffer, mGraph);
            }
        }
        else
        {
            mGraph.renderSimple(mSceneTree);
        }
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
        }
        catch (const std::exception & aException)
        {
            ADLOG(error)("Exception thrown while compiling technique:\n{}",
                         aException.what());
        }
    }

    // Scene control
    ImGui::Checkbox("Show Punctual Lights", &mSceneControl.mShowPunctualLights);
    ImGui::Checkbox("Draw BB", &mSceneControl.mDrawBoundingBoxes);

    ImGui::SeparatorText("Voxelization:");
    mVoxelizationRequest |= ImGui::Checkbox("Dominant Axis Method", &mVoxelizer.mControl.mUseDominantAxis);
    mVoxelizationRequest |= ImGui::Checkbox("Conservative Rasterization", &mVoxelizer.mControl.mConservativeRasterization);
    mVoxelizationRequest |= ImGui::Checkbox("Conservative Depth Range", &mVoxelizer.mControl.mConservativeDepthRange);
    ImGui::Checkbox("Show Voxels", &mSceneControl.mShowVoxels);
    ImGui::Indent();
    {
        if (!mSceneControl.mShowVoxels) ImGui::BeginDisabled();
        ImGui::Checkbox("Raytrace Voxels", &mSceneControl.mRaytraceVoxels);
        if (!mSceneControl.mShowVoxels) ImGui::EndDisabled();
    }
    ImGui::Unindent();
    ImGui::Checkbox("Voxel POV", &mSceneControl.mVoxelPov);

    DearImguiWitness witness;

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Frame Graph"))
    {
        mGraph.appendUi();
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Materials"))
    {
        describe(witness, mMaterials);
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

} // namespace ad
