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

#include <scenic/ColorPalettes.h>
#include <scenic/LoadScene.h>

#include <ui/ImguiUi.h>
#include <ui/Widgets.h>
#include <ui/Widgets-impl.h>


namespace ad {


// TODO: merge back to graphics
template <class T_Pixel>
void serializeTexture(const graphics::Texture& aTexture,
                      GLint aLevel,
                      GLenum aPixelFormat,
                      arte::ImageFormat aFormat,
                      std::ostream& aOut)
{
    graphics::ScopedBind boundTexture{ aTexture };

    math::Size<2, GLint> size;
    glGetTexLevelParameteriv(aTexture.mTarget,
                             aLevel,
                             GL_TEXTURE_WIDTH,
                             &size.width());
    glGetTexLevelParameteriv(aTexture.mTarget,
                             aLevel,
                             GL_TEXTURE_HEIGHT,
                             &size.height());

    // TODO: retrieve the texture internal format, and assert T_Pixel compatibility
    //GLenum internalFormat;
    //glGetTexLevelParameteriv(aTexture.mTarget,
    //                         aLevel,
    //                         GL_TEXTURE_INTERNAL_FORMAT,
    //                         static_cast<GLint *>(&internalFormat));

    // Note: All image format we can write to accept 1-byte alignment for rows,
    // and STBI_writer only allow to control the stride for PNG.
    // Default OpenGL value is 4-bytes alignment for row start, which can be problematic
    // for < 4 components image with a width that is not a multiple of 4.
    // The easy solution is to always require 1-byte alignment 
    // (even when it gives the same results than 4-bytes alignment)
    auto packAlignmentGuard = graphics::scopePackAlignment(1);

    std::unique_ptr<unsigned char[]> raster =
        std::make_unique<unsigned char[]>(sizeof(T_Pixel) * size.area());

    glGetTexImage(aTexture.mTarget,
                  aLevel,
                  aPixelFormat,
                  graphics::MappedPixelComponentType_v<T_Pixel>,
                  raster.get());

    arte::Image<T_Pixel> result{ size, std::move(raster) };
    result.write(aFormat, aOut);
}


void loadToBuffer(const renderer::EntitiesBlock_glsl& aData,
                  const graphics::UniformBufferObject& aBuffer,
                  graphics::BufferHint aUsageHint)
{
    graphics::load(aBuffer, std::span{ aData.mEntities }, aUsageHint);
}


// The integration demo, lighting a sphere from a polygon
const std::filesystem::path gSurfaceProgramPath = "programs/ch11_global_illumination_Pbr.prog";
const std::filesystem::path gLightProgramPath = "programs/RenderModel_PlainColor.prog";

const std::filesystem::path gModelPaths[] = { "models/Mat/meetmat_2.glb" };
constexpr float gModelScale = 0.1f;

//const std::filesystem::path gModelPaths[] = {
//    "models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/head.stl",
//    "models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/body.stl",
//    "models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/tail-1.stl",
//    "models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/tail-2.stl",
//    "models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/leg-l.stl",
//    "models/Glavenus/6286129a92b31_glavenus-rpg-scale-fan-art/leg-r.stl",
//};
//constexpr float gModelScale = 0.01f;

const renderer::ReferencePath gEnvMapPath{ "envmaps/neon_photostudio/neon_photostudio_8k-cubemap.dds" };


scenic::SceneTree prepareSceneTree(Engine & aEngine)
{
    scenic::SceneTree result;
    for (const auto & path : gModelPaths)
    {
        scenic::loadModel(result,
                          aEngine.mLoader.mFinder.pathFor(path),
                          aEngine.mContext,
                          gModelScale);
    }
    return result;
}


// TODO: on framebuffer resize, inform the framegraph
Scene::Scene(graphics::AppInterface& aAppInterface, const imguiui::ImguiUi& aImgui) :
    mSurfaceProgram{ mGraph.mEngine.loadProgram(renderer::ReferencePath{gSurfaceProgramPath}) },
    mLightProgram{ mGraph.mEngine.loadProgram(renderer::ReferencePath{gLightProgramPath}) },
    mGraph(aAppInterface.getFramebufferSize()),
    mSceneTree{prepareSceneTree(mGraph.mEngine)},
    mEnvironment{ scenic::prepareEnvironment(gEnvMapPath, mGraph.mEngine.mLoader) }
{
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


void Scene::loadPrograms()
{
    mGraph.loadPrograms();
    mSurfaceProgram =
        mGraph.mEngine.loadProgram(renderer::ReferencePath{ gSurfaceProgramPath });
    mLightProgram =
        mGraph.mEngine.loadProgram(renderer::ReferencePath{ gLightProgramPath });
}


void Scene::step(const graphics::Timer& /*aTimer*/,
                 math::Size<2, int> aWindowResolution)
{
    mOrbitalCamera.update(aWindowResolution.height());
}


// TODO: move to a generic header
renderer::LightsDataCommon transformLightsData(
    renderer::LightsDataCommon aLightsData, // by value, as we need a copy
    const math::AffineMatrix<4, float>& aTransform)
{
    for (auto idx = 0; idx != aLightsData.mDirectionalCount; ++idx)
    {
        renderer::DirectionalLight_glsl& light = aLightsData.mDirectionalLights[idx];
        // might be unecessary to re-normalize, unless the transform scales
        light.mDirection = math::UnitVec<3, GLfloat>{
            light.mDirection * aTransform.getLinear() };
    }
    for (auto idx = 0; idx != aLightsData.mPointCount; ++idx)
    {
        renderer::PointLight_glsl& light = aLightsData.mPointLights[idx];
        light.mPosition = math::homogeneous::homogenize(
            math::homogeneous::makePosition(light.mPosition) * aTransform).xyz();
    }

    return aLightsData;
}


void Scene::render(math::Size<2, int> aRenderResolution)
{
    //
    // Entities
    // 
    const unsigned int objectsCount = mSceneTree.mObjectsMap.size();
    // Ensure the vector can fit all objects and point lights
    mEntities.mEntities.resize(objectsCount + mLights.mPointCount);

    std::size_t objectIdx = 0;
    for (const auto& [nodeIdx, object] : mSceneTree.mObjectsMap)
    {
        auto& entity = mEntities.mEntities[objectIdx];
        entity.mLocalToWorld = static_cast<math::AffineMatrix<4, GLfloat>>(
            mSceneTree.mTree.mGlobalPose[nodeIdx]);
        ++objectIdx;
    }

    for (std::size_t lightIdx = 0; lightIdx != mLights.mPointCount; ++lightIdx)
    {
        const auto& light = mLights.mPointLights[lightIdx];
        auto& entity = mEntities.mEntities[objectsCount + lightIdx];
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
    mOrbitalCamera.setRatio(math::getRatio<GLfloat>(aRenderResolution));
    graphics::loadSingle(mViewProjectionBuffer,
                         mOrbitalCamera.getViewProjectionBlock(),
                         graphics::BufferHint::StreamDraw);

    //
    // Frame rendering
    //
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, aRenderResolution.width(), aRenderResolution.height());
    glClearColor(0.1f, 0.2f, 0.3f, 1.f); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mGraph.renderFrame(mSceneTree, mEnvironment, aRenderResolution);

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
        glBlitNamedFramebuffer(mGraph.mFbo, 0,
                               0, 0, aRenderResolution.width(), aRenderResolution.height(),
                               0, 0, aRenderResolution.width(), aRenderResolution.height(),
                               GL_COLOR_BUFFER_BIT,
                               GL_NEAREST);
        // Depth must also be copied for lights occlusion
        glBlitNamedFramebuffer(mGraph.mFbo, 0,
                               0, 0, aRenderResolution.width(), aRenderResolution.height(),
                               0, 0, aRenderResolution.width(), aRenderResolution.height(),
                               GL_DEPTH_BUFFER_BIT,
                               GL_NEAREST);

        //
        // Draw lights
        //
        if (mSceneControl.mShowPunctualLights)
        {
            glUseProgram(mLightProgram);

            for (const scenic::MeshPart_Naive& part : mSphere.mParts)
            {
                graphics::VertexArrayObject vao = prepareVAO(mLightProgram, part);
                glBindVertexArray(vao);

                if (scenic::useElementIndices(part))
                {
                    glDrawElementsInstancedBaseInstance(
                        part.mPrimitiveMode,
                        part.mIndicesCount,
                        part.mIndicesType,
                        (void*)part.mIndexFirst,
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


void Scene::presentUi(bool* aOpen)
{
    ImGui::Begin("Scene", aOpen);

    if (ImGui::Button("Recompile shaders"))
    {
        try
        {
            loadPrograms();
        }
        catch (const std::exception& aException)
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
    ImGui::End();
}

}
