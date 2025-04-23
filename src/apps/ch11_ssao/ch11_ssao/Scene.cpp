#include "Scene.h"

#include "log/Logging.h"

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


// TODO: merge back to graphics
template <class T_Pixel>
void serializeTexture(const graphics::Texture & aTexture,
                      GLint aLevel,
                      GLenum aPixelFormat,
                      arte::ImageFormat aFormat,
                      std::ostream & aOut,
                      arte::ImageOrientation aOrientation = arte::ImageOrientation::Unchanged)
{
    graphics::ScopedBind boundTexture{ aTexture };

    const bool isCubemap = aTexture.mTarget == GL_TEXTURE_CUBE_MAP;
    GLenum target = isCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : aTexture.mTarget;
    GLint faceCount = isCubemap ? 6 : 1;

    math::Size<2, GLint> faceSize;
    glGetTexLevelParameteriv(target,
                             aLevel,
                             GL_TEXTURE_WIDTH,
                             &faceSize.width());
    glGetTexLevelParameteriv(target,
                             aLevel,
                             GL_TEXTURE_HEIGHT,
                             &faceSize.height());

    // For a cubemap, dump the 6 faces as an horizontal strip
    math::Size<2, GLint> resultSize = faceSize.cwMul({ faceCount, 1 });

    // TODO: retrieve the texture internal format, and assert T_Pixel compatibility
    //GLenum internalFormat;
    //glGetTexLevelParameteriv(target,
    //                         aLevel,
    //                         GL_TEXTURE_INTERNAL_FORMAT,
    //                         static_cast<GLint *>(&internalFormat));
    std::unique_ptr<unsigned char[]> raster =
        std::make_unique<unsigned char[]>(sizeof(T_Pixel) * resultSize.area());

    // Note: All image format we can write to accept 1-byte alignment for rows,
    // and STBI_writer only allow to control the stride for PNG.
    // Default OpenGL value is 4-bytes alignment for row start, which can be problematic
    // for < 4 components image with a width that is not a multiple of 4.
    // The easy solution is to always require 1-byte alignment 
    // (even when it gives the same results than 4-bytes alignment)
    auto scopePackAlignment = graphics::scopePackAlignment(1);
    // For cubemap strip, we have to define the number of pixels in each row to the total strip width
    auto scopedRowLength = graphics::scopePixelStorageMode(GL_PACK_ROW_LENGTH, 
                                                           isCubemap ? resultSize.width() : 0);

    // For non-cubemap, the loop body will execute only once, with offset == 0
    for (unsigned int offset = 0; offset != faceCount; ++offset)
    {
        glGetTexImage(target + offset,
                      aLevel,
                      aPixelFormat,
                      graphics::MappedPixelComponentType_v<T_Pixel>,
                      raster.get() + (offset * faceSize.width() * sizeof(T_Pixel)));
    }

    arte::Image<T_Pixel> result{ resultSize, std::move(raster) };
    result.write(aFormat, aOut, aOrientation);
}


void loadToBuffer(const renderer::EntitiesBlock_glsl & aData,
                  const graphics::UniformBufferObject & aBuffer,
                  graphics::BufferHint aUsageHint)
{
    graphics::load(aBuffer, std::span{ aData.mEntities }, aUsageHint);
}


// The integration demo, lighting a sphere from a polygon
const std::filesystem::path gSurfaceProgramPath = "programs/ch11_global_illumination_Pbr.prog";
const std::filesystem::path gLightProgramPath = "programs/RenderModel_PlainColor.prog";

//const std::filesystem::path gModelPaths[] = { "models/Mat/meetmat_2.glb" };
//constexpr float gModelScale = 0.1f;

const renderer::ReferencePath gModelPaths[] = {
    renderer::ReferencePath{"models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/head.stl"},
    renderer::ReferencePath{"models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/body.stl"},
    renderer::ReferencePath{"models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/body-horn-l.stl"},
    renderer::ReferencePath{"models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/body-horn-r.stl"},
    renderer::ReferencePath{"models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/tail-1.stl"},
    renderer::ReferencePath{"models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/tail-2.stl"},
    renderer::ReferencePath{"models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/leg-l.stl"},
    renderer::ReferencePath{"models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/leg-r.stl"},
};
constexpr float gModelScale = 0.01f;

//const renderer::ReferencePath gEnvMapPath{ "envmaps/neon_photostudio/neon_photostudio_8k-cubemap.dds" };
//const renderer::ReferencePath gEnvMapPath{ "envmaps/winter_evening/winter_evening_8k.hdr" };
//const renderer::ReferencePath gEnvMapPath{ "envmaps/rogland_clear_night/rogland_clear_night_8k.hdr" };
const renderer::ReferencePath gEnvMapPath{ "envmaps/rostock_arches/rostock_arches_8k.dds" };



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
    mSurfaceProgram{ mGraph.mEngine.loadProgram(renderer::ReferencePath{gSurfaceProgramPath}) },
    mLightProgram{ mGraph.mEngine.loadProgram(renderer::ReferencePath{gLightProgramPath}) },
    mGraph(aAppInterface.getFramebufferSize()),
    mSceneTree{ prepareSceneTree(mGraph.mEngine) },
    mEnvironment{ scenic::prepareEnvironment(gEnvMapPath, mGraph.mEngine.mLoader) }
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
    mSurfaceProgram =
        mGraph.mEngine.loadProgram(renderer::ReferencePath{ gSurfaceProgramPath });
    mLightProgram =
        mGraph.mEngine.loadProgram(renderer::ReferencePath{ gLightProgramPath });
}


void Scene::step(const graphics::Timer & /*aTimer*/,
                 math::Size<2, int> aWindowResolution)
{
    mOrbitalCamera.update(aWindowResolution.height());
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


void Scene::render(math::Size<2, int> aBackbufferResolution)
{
    renderTo(graphics::FrameBuffer::Default(), aBackbufferResolution);
}


void Scene::renderTo(const graphics::FrameBuffer & aFramebuffer, math::Size<2, int> aBackbufferResolution)
{
    //
    // Entities
    // 
    const unsigned int objectsCount = mSceneTree.mObjectsMap.size();
    // Ensure the vector can fit all objects and point lights
    mEntities.mEntities.resize(objectsCount + mLights.mPointCount);

    std::size_t objectIdx = 0;
    for (const auto & [nodeIdx, object] : mSceneTree.mObjectsMap)
    {
        auto & entity = mEntities.mEntities[objectIdx];
        entity.mLocalToWorld = static_cast<math::AffineMatrix<4, GLfloat>>(
            mSceneTree.mTree.mGlobalPose[nodeIdx]);
        ++objectIdx;
    }

    for (std::size_t lightIdx = 0; lightIdx != mLights.mPointCount; ++lightIdx)
    {
        const auto & light = mLights.mPointLights[lightIdx];
        auto & entity = mEntities.mEntities[objectsCount + lightIdx];
        entity.mLocalToWorld =
            math::trans3d::scaleUniform(light.mRadius.mMin)
            * math::trans3d::translate(light.mPosition.as<math::Vec>());
        entity.mColorFactor = light.mColors.mSpecularColor;
    }
    loadToBuffer(mEntities, mEntitiesBlockBuffer, graphics::BufferHint::StreamDraw);

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
    // Camera
    //
    changeAspectRatio(mOrbitalCamera.mCamera, math::getRatio<GLfloat>(aBackbufferResolution));
    graphics::loadSingle(mViewProjectionBuffer,
                         mOrbitalCamera.getViewProjectionBlock(),
                         graphics::BufferHint::StreamDraw);

    //
    // Frame rendering
    //
    mGraph.renderFrame(mSceneTree, mEnvironment);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, aFramebuffer);
    glViewport(0, 0, aBackbufferResolution.width(), aBackbufferResolution.height());
    glClearColor(0.1f, 0.2f, 0.3f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mSceneControl.mShowTexture)
    {
        mGraph.passShowTexture(mOrbitalCamera.mCamera,
                               mSceneControl.mTexture);
    }
    else
    {
        // Smell: We rely on the knowing last color attachment 
        // In a production setup, it is likely that the framegraph would render the final
        // frame to a provided framebuffer.
        glNamedFramebufferReadBuffer(mGraph.mFbo, GL_COLOR_ATTACHMENT0);
        glBlitNamedFramebuffer(mGraph.mFbo, aFramebuffer,
                               0, 0, mGraph.mTextures.mScreenTextureSize.width(), mGraph.mTextures.mScreenTextureSize.height(),
                               0, 0, aBackbufferResolution.width(), aBackbufferResolution.height(),
                               GL_COLOR_BUFFER_BIT,
                               GL_NEAREST);
        // Depth must also be copied for lights occlusion
        glBlitNamedFramebuffer(mGraph.mFbo, aFramebuffer,
                               0, 0, mGraph.mTextures.mScreenTextureSize.width(), mGraph.mTextures.mScreenTextureSize.height(),
                               0, 0, aBackbufferResolution.width(), aBackbufferResolution.height(),
                               GL_DEPTH_BUFFER_BIT,
                               GL_NEAREST);

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
                        objectsCount /* base instance, light instances are after objects in UBOs */);
                }
                else
                {
                    throw std::logic_error{ "Who is not using indexed rendering?" };
                }
            }
        }
    }
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
    ImGui::Checkbox("Show Texture", &mSceneControl.mShowTexture);
    imguiui::addCombo("Texture",
                      mSceneControl.mTexture,
                      std::span{ TextureStore::gNames });

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

    if (ImGui::Button("Dump depth map"))
    {
        std::ofstream outFile{ "rtr_11-depth_texture.png", std::ios::binary };
        if (!outFile.good())
        {
            throw std::runtime_error{ "Cannot open output file." };
        }
        ad::serializeTexture<math::sdr::Grayscale>(mGraph.tex(TextureStore::DepthMap), 0, GL_DEPTH_COMPONENT,
                                                   arte::ImageFormat::Png, outFile);
    }
    if (ImGui::Button("Dump position map"))
    {
        std::ofstream outFile{ "rtr_11-position_texture.png", std::ios::binary };
        if (!outFile.good())
        {
            throw std::runtime_error{ "Cannot open output file." };
        }
        ad::serializeTexture<math::sdr::Rgb>(mGraph.tex(TextureStore::FragPositionView), 0, GL_RGB,
                                             arte::ImageFormat::Png, outFile);
    }
    if (ImGui::Button("Dump cube map"))
    {
        std::ofstream outFile{ "rtr_11-envmap-strip.hdr", std::ios::binary };
        if (!outFile.good())
        {
            throw std::runtime_error{ "Cannot open output file." };
        }
        ad::serializeTexture<math::hdr::Rgb_f>(mEnvironment.mEnvMap.mTexture, 0, GL_RGB,
                                             arte::ImageFormat::Hdr, outFile);
    }
    if (ImGui::Button("Dump 4K frame"))
    {
        std::ofstream outFile{ "rtr_11-frame-4k.png", std::ios::binary };
        if (!outFile.good())
        {
            throw std::runtime_error{ "Cannot open output file." };
        }
        auto savedSize = mGraph.mTextures.mScreenTextureSize;

        math::Size<2, int> fullhd{ 1920, 1200 };
        math::Size<2, int> resolution = 2*fullhd;
        onFramebufferResize(resolution);
        graphics::FrameBuffer fbo;
        graphics::ScopedBind{fbo}; // Just to create it
        graphics::Texture colorTarget{GL_TEXTURE_2D};
        graphics::ScopedBind{colorTarget}; // Just to create it
        glTextureStorage2D(colorTarget,
                           1,
                           GL_RGB8,
                           resolution.width(),
                           resolution.height());
        glNamedFramebufferTexture(fbo,
                                  GL_COLOR_ATTACHMENT0,
                                  colorTarget,
                                  /*mip map level*/0);
        renderTo(fbo, resolution);
        ad::serializeTexture<math::sdr::Rgb>(colorTarget, 0, GL_RGB,
                                             arte::ImageFormat::Png, outFile,
                                             arte::ImageOrientation::InvertVerticalAxis);
        onFramebufferResize(savedSize);
    }
    ImGui::End();
}

} // namespace ad
