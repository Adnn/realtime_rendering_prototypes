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

void loadToBuffer(const renderer::EntitiesBlock_glsl & aData,
                  const graphics::UniformBufferObject & aBuffer,
                  graphics::BufferHint aUsageHint)
{
    graphics::load(aBuffer, std::span{ aData.mEntities }, aUsageHint);
}


// The integration demo, lighting a sphere from a polygon
const std::filesystem::path gSurfaceProgramPath = "programs/RenderModel_PlainColor.prog";

const std::filesystem::path gModelPath = "models/Mat/meetmat_2.glb";
constexpr float gModelScale = 0.1f;


Scene::Scene(graphics::AppInterface & aAppInterface, const imguiui::ImguiUi & aImgui) :
    mSurfaceProgram{mEngine.loadProgram(renderer::ReferencePath{gSurfaceProgramPath})},
    mSceneTree{ scenic::loadModel(mEngine.mLoader.mFinder.pathFor(gModelPath),
                                  mEngine.mContext,
                                  gModelScale) }
{
    // Register the camera system with glfw inputs 
    graphics::registerGlfwCallbacks(
        aAppInterface,
        mOrbitalCamera.mOrbitalControl,
        graphics::EscKeyBehaviour::Close,
        // TODO: this is a dirty capture of a parameter given by reference
        &aImgui);

    // TODO use defines here for binding points
    graphics::bind(mViewProjectionBuffer, graphics::BindingIndex{0});
    glObjectLabel(GL_BUFFER, mViewProjectionBuffer, -1, "ViewProjection");
    graphics::bind(mEntitiesBlockBuffer, graphics::BindingIndex{1});
    glObjectLabel(GL_BUFFER, mEntitiesBlockBuffer, -1, "Entities");
    graphics::bind(mMaterialsBlockBuffer, graphics::BindingIndex{2});
    glObjectLabel(GL_BUFFER, mMaterialsBlockBuffer, -1, "Materials");
    graphics::bind(mLightsBlockBuffer, graphics::BindingIndex{4});
    glObjectLabel(GL_BUFFER, mLightsBlockBuffer, -1, "Lights");
}


void Scene::loadPrograms()
{
    mSurfaceProgram =
        mEngine.loadProgram(renderer::ReferencePath{ gSurfaceProgramPath });
}


void Scene::step(const graphics::Timer & /*aTimer*/,
                 math::Size<2, int> aWindowResolution)
{
    mOrbitalCamera.update(aWindowResolution.height());
}


void Scene::render(math::Size<2, int> aRenderResolution)
{
    //
    // Entities
    // 
    //const unsigned int objectsCount = 0;
    //// Ensure the vector can fit all point lights

    //mEntities.mEntities.resize(objectsCount + mLights.mPlanarCount);

    //for (std::size_t lightIdx = 0; lightIdx != mLights.mPlanarCount; ++lightIdx)
    //{
    //    const auto& light = mLights.mPlanarLights[lightIdx];
    //    auto& entity = mEntities.mEntities[objectsCount + lightIdx];
    //    entity.mLocalToWorld =
    //        math::trans3d::scale(
    //            light.mRect.mDimension.width(),
    //            1.f,
    //            light.mRect.mDimension.height())
    //        * math::trans3d::translate<GLfloat>({
    //            light.mRect.mPosition.x(),
    //            light.mHeight,
    //            light.mRect.y()});
    //    entity.mColorFactor = light.mColors.mSpecularColor;
    //}
    loadToBuffer(mEntities, mEntitiesBlockBuffer, graphics::BufferHint::StreamDraw);

    //
    // Materials
    // 
    graphics::loadSingle(mMaterialsBlockBuffer, mMaterials, graphics::BufferHint::StreamDraw);

    //
    // Lights
    //
    //graphics::loadSingle(mLightsBlockBuffer, mLights, graphics::BufferHint::StreamDraw);

    //
    // Camera
    //
    mOrbitalCamera.setRatio(math::getRatio<GLfloat>(aRenderResolution));
    graphics::loadSingle(mViewProjectionBuffer,
                         mOrbitalCamera.getViewProjectionBlock(),
                         graphics::BufferHint::StreamDraw);

    //
    // Draw
    //

    // Pipeline state
    glPolygonMode(GL_FRONT_AND_BACK, *mPipelineControl.mPolygonMode);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, aRenderResolution.width(), aRenderResolution.height());

    // Program
    glUseProgram(mSurfaceProgram);

    for (const auto & [nodeIdx, object] : mSceneTree.mObjectsMap)
    {
        for (const scenic::MeshPart_Naive & part : object.mParts)
        {
            graphics::VertexArrayObject vao = prepareVAO(mSurfaceProgram, part);
            glBindVertexArray(vao);

            if (scenic::useElementIndices(part))
            {
                glDrawElementsInstancedBaseInstance(
                    part.mPrimitiveMode,
                    part.mIndicesCount,
                    part.mIndicesType,
                    (void *)part.mIndexFirst,
                    1, // One instance
                    0 /* base instance */);
            }
            else
            {
                throw std::logic_error{ "Who is not using indexed rendering?" };
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
        catch (const std::exception& aException)
        {
            ADLOG(error)("Exception thrown while compiling technique:\n{}",
                         aException.what());
        }
    }

    imguiui::addCombo("Polygon mode",
        mPipelineControl.mPolygonMode,
        PipelineControl::gPolygonModes.begin(),
        PipelineControl::gPolygonModes.end(),
        [](auto aModeIt){return graphics::to_string(*aModeIt);});

    DearImguiWitness witness;

    //ImGui::Spacing();
    if (ImGui::CollapsingHeader("Materials"))
    {
        describe(witness, mMaterials);
    }

    ImGui::End();
}


} // namespace ad
