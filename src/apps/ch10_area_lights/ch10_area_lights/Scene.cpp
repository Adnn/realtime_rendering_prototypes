#include "Scene.h"

#include "log/Logging.h"

#include <graphics/AppInterface.h>
#include <graphics/ApplicationGlfw.h>
#include <graphics/CameraUtilities.h>

#include <reflect/DearImguiWitness.h>
#include <reflect/ReflectHelpers.h>

#include <renderer/BufferIndexedBinding.h>
#include <renderer/BufferLoad.h>

#include <ui/ImguiUi.h>
#include <ui/Widgets.h>
#include <ui/Widgets-impl.h>


namespace ad {


template <class T_witness>
void describe(T_witness aWitness, Scene::TessellationControl & aValue)
{
    GIVE_EX(make_Clamped(aValue.mPatchVertices, {.mMin = 1u, .mMax = (GLuint)aValue.mMaxPatchVertices}), 
            PatchVertices);
    GIVE(OuterLevel);
    GIVE(InnerLevel);

    static const math::Vec<4, GLfloat> maxTess{
        (GLfloat)aValue.mMaxTessGenLevel,
        (GLfloat)aValue.mMaxTessGenLevel,
        (GLfloat)aValue.mMaxTessGenLevel,
        (GLfloat)aValue.mMaxTessGenLevel,
    };
    aValue.mOuterLevel = math::min(aValue.mOuterLevel, maxTess);
    aValue.mInnerLevel = math::min(aValue.mInnerLevel, maxTess.xy());
}


Scene::Scene(graphics::AppInterface & aAppInterface, const imguiui::ImguiUi & aImgui) :
    mEngine{
        .mLoader = renderer::makeResourceFinder()
    },
    mVertexSpecification{},
    mIndexBuffer{
        graphics::loadIndexBuffer(mVertexSpecification.mVertexArray,
                                  std::span{scenic::icosahedron::gIndices},
                                  graphics::BufferHint::StaticDraw)},
    mIntrospectProgram{mEngine.mLoader.loadProgram(renderer::ReferencePath{"programs/TessellateSphere.prog"})}
{
    graphics::attachIndexBuffer(mIndexBuffer, mVertexSpecification.mVertexArray);

    graphics::appendToVertexSpecification(
        mVertexSpecification,
        gVertexDescription,
        std::span{scenic::icosahedron::gPositions},
        graphics::BufferHint::StaticDraw);

    graphics::appendToVertexSpecification(
        mVertexSpecification,
        gInstanceDescription,
        std::span{gInstances},
        graphics::BufferHint::StaticDraw,
        1);

    // Register the camera system with glfw inputs 
    graphics::registerGlfwCallbacks(
        aAppInterface,
        mOrbitalCamera.mOrbitalControl,
        graphics::EscKeyBehaviour::Close,
        // TODO: this is a dirty capture of a parameter given by reference
        &aImgui);
}


void Scene::step(const graphics::Timer & /*aTimer*/,
                 math::Size<2, int> aWindowResolution)
{
    mOrbitalCamera.update(aWindowResolution.height());
}



void Scene::render(math::Size<2, int> aRenderResolution)
{
    glPolygonMode(GL_FRONT_AND_BACK, *mPipelineControl.mPolygonMode);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(mVertexSpecification.mVertexArray);
    glUseProgram(mIntrospectProgram);

    mOrbitalCamera.setRatio(math::getRatio<GLfloat>(aRenderResolution));
    // TODO use defines here for binding points
    graphics::bind(mViewProjectionBuffer, graphics::BindingIndex{0});
    graphics::loadSingle(mViewProjectionBuffer,
                         mOrbitalCamera.getViewProjectionBlock(),
                         graphics::BufferHint::StreamDraw);

    glViewport(0, 0, aRenderResolution.width(), aRenderResolution.height());

    // The input patch (directly fed to the TES) are the 3 vertices of a triangle.
    glPatchParameteri(GL_PATCH_VERTICES, mTessControl.mPatchVertices);
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, mTessControl.mOuterLevel.data());
    glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, mTessControl.mInnerLevel.data());

    glDrawElementsInstanced(
        GL_PATCHES,
        static_cast<GLsizei>(std::size(scenic::icosahedron::gIndices)),
        graphics::MappedGL_v<std::remove_cvref_t<
            decltype(*scenic::icosahedron::gIndices)>>,
        0,
        static_cast<GLsizei>(std::size(gInstances)));
}


void Scene::presentUi(bool * aOpen)
{
    ImGui::Begin("Scene", aOpen);

    if (ImGui::Button("Recompile shaders"))
    {
        try
        {
            mIntrospectProgram =
                mEngine.mLoader.loadProgram(renderer::ReferencePath{ "programs/TessellateSphere.prog" });
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
    describe(witness, mTessControl);
    ImGui::End();
}


} // namespace ad
