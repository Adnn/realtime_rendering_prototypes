#include "Scene.h"

#include "shaders.h"

#include <graphics/AppInterface.h>
#include <graphics/ApplicationGlfw.h>
#include <graphics/CameraUtilities.h>

#include <renderer/BufferIndexedBinding.h>
#include <renderer/BufferLoad.h>


namespace ad {


Scene::Scene(graphics::AppInterface & aAppInterface) :
    mVertexSpecification{},
    mIndexBuffer{
        graphics::loadIndexBuffer(mVertexSpecification.mVertexArray,
                                  std::span{scenic::icosahedron::gIndices},
                                  graphics::BufferHint::StaticDraw)},
    mProgram{graphics::makeLinkedProgram({
              {GL_VERTEX_SHADER,   gVertexShader},
              {GL_FRAGMENT_SHADER, gFragmentShader},
              {GL_TESS_EVALUATION_SHADER, gTessellationEvaluationShader},
    })}
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
        // TODO: this is the default value, but apparently it does not deduce it
        // try to make the matching type default template type argument.
        &graphics::NullInhibiter::gInstance);
}


void Scene::step(const graphics::Timer & /*aTimer*/,
                 math::Size<2, int> aWindowResolution)
{
    mOrbitalCamera.update(aWindowResolution.height());
}



void Scene::render(math::Size<2, int> aRenderResolution)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glBindVertexArray(mVertexSpecification.mVertexArray);
    glUseProgram(mProgram);

    mOrbitalCamera.setRatio(math::getRatio<GLfloat>(aRenderResolution));
    // TODO use defines here for binding points
    graphics::bind(mViewProjectionBuffer, graphics::BindingIndex{0});
    graphics::loadSingle(mViewProjectionBuffer,
                         mOrbitalCamera.getViewProjectionBlock(),
                         graphics::BufferHint::StreamDraw);

    glViewport(0, 0, aRenderResolution.width(), aRenderResolution.height());

    // The input patch (directly fed to the TES) are the 3 vertices of a triangle.
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    const GLfloat outer[] = { 2, 2, 2 };
    const GLfloat inner[] = { 2 };
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outer);
    glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, inner);

    glDrawElementsInstanced(
        GL_PATCHES,
        static_cast<GLsizei>(std::size(scenic::icosahedron::gIndices)),
        graphics::MappedGL_v<std::remove_cvref_t<
            decltype(*scenic::icosahedron::gIndices)>>,
        0,
        static_cast<GLsizei>(std::size(gInstances)));
}


} // namespace ad
