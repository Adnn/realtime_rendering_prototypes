#include "FrameGraph.h"

#include "log/Logging.h"
#include "SetupDrawing.h"
#include "UniformSetterWitness.h"

#include <handy/vector_utils.h>

#include <reflect/DearImguiWitness.h>
#include <reflect/ReflectHelpers.h>

#include <renderer/Uniforms.h>

#include <scenic/Camera.h>

#include <scenic/environment/Skybox.h>
#include <scenic/environment/EnvironmentUtilities.h>

#include <ui/Widgets-impl.h>

#include <random>


namespace ad {


    namespace {


        const std::filesystem::path gBlinnPhongProgramPath = "programs/RenderModel_BlinnPhong.prog";


    } // unnamed namespace



void drawPass(const renderer::IntrospectProgram & aProgram, 
              const scenic::SceneTree & aSceneTree)
{
    // We populated the per-instance buffer in the order of the objects map iteration
    // We keep track of the base-instance to access the correct index in the shader
    GLuint baseInstance = 0;
    const GLuint instanceCount = 1;
    glUseProgram(aProgram);
    for (const auto & [nodeIdx, object] : aSceneTree.mObjectsMap)
    {
        for (const scenic::MeshPart_Naive & part : object.mParts)
        {
            graphics::VertexArrayObject vao = prepareVAO(aProgram, part);
            glBindVertexArray(vao);

            if (scenic::useElementIndices(part))
            {
                glDrawElementsInstancedBaseInstance(
                    part.mPrimitiveMode,
                    part.mIndicesCount,
                    part.mIndicesType,
                    (void *)part.mIndexFirst,
                    instanceCount, // One instance
                    baseInstance);
            }
            else
            {
                throw std::logic_error{ "Who is not using indexed rendering?" };
            }
            baseInstance += instanceCount;
        }
    }
}


FrameGraph::ProgramStore::ProgramStore(Engine & aEngine) :
    mBlinnPhong{ aEngine.loadProgram(renderer::ReferencePath{ gBlinnPhongProgramPath }) }
{}


FrameGraph::FrameGraph(math::Size<2, int> aFrameSize) :
    mPrograms{mEngine}
{

    //
    // FBO attachments
    //
    //{
    //    graphics::ScopedBind boundFbo{mFbo, graphics::FrameBufferTarget::Draw};
    //    // The texture attachment is permanent, no need to recreate it each time the FBO is bound
    //    glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
    //                         GL_DEPTH_ATTACHMENT,
    //                         tex(TextureStore::DepthMap),
    //                         /*mip map level*/0);

    //    assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    //}

    // Dummy VAO
    graphics::ScopedBind{ mDummyVao };
    glObjectLabel(GL_VERTEX_ARRAY, mDummyVao, -1, "dummy_vao");
}


void FrameGraph::resizeFrame(math::Size<2, int> aRenderResolution)
{
}


void FrameGraph::loadPrograms()
{ 
    mPrograms = ProgramStore{mEngine}; 
}


void FrameGraph::renderSimple(const scenic::SceneTree & aSceneTree)
{
    passBlinnPhong(aSceneTree);
}


void FrameGraph::passBlinnPhong(const scenic::SceneTree & aSceneTree)
{
    glPolygonMode(GL_FRONT_AND_BACK, *mFrameControl.mPolygonMode);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    const auto& program = mPrograms.mBlinnPhong;

    drawPass(program, aSceneTree);
}


void FrameGraph::appendUi()
{
    DearImguiWitness witness;

    imguiui::addCombo("Polygon mode",
                      mFrameControl.mPolygonMode,
                      FrameControl::gPolygonModes.begin(),
                      FrameControl::gPolygonModes.end(),
                      [](auto aModeIt) {return graphics::to_string(*aModeIt); });
}


} // namespace ad