#include "FrameGraph.h"

#include "SetupDrawing.h"
#include "UniformSetterWitness.h"
#include "Voxelization.h"

#include "log/Logging.h"

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


        const std::filesystem::path gBlinnPhongProgramPath = "programs/ch11_RenderModel_BlinnPhong.prog";
        const std::filesystem::path gConeTraceProgramPath = "programs/ch11_ConeTrace.prog";
        const std::filesystem::path gRayTraceVoxelsProgramPath = "programs/ch11_RayTraceVoxels.prog";

        const renderer::ReferencePath gVoxelizationProgram{"programs/ch11_Voxelization.prog"};
        const renderer::ReferencePath gVoxelizationDominantAxisProgram{"programs/ch11_VoxelizationDominantAxis.prog"};
        const renderer::ReferencePath gVoxelizationViewProgram{"programs/ch11_View.prog"};
        const renderer::ReferencePath gVoxelizationDominantAxisViewProgram{"programs/ch11_ViewDominantAxis.prog"};


    } // unnamed namespace



void drawPass(const renderer::IntrospectProgram & aProgram, 
              const scenic::SceneTree & aSceneTree,
              const Engine & aEngine)
{
    // We populated the per-instance buffer in the order of the objects map iteration
    // We keep track of the base-instance to access the correct index in the shader
    GLuint baseInstance = 0;
    const GLuint instanceCount = 1;
    glUseProgram(aProgram);

    const GLuint diffuseTextureUnit = 0;
    graphics::setUniform(aProgram, "u_AlbedoTexture", diffuseTextureUnit);

    for (const auto & [nodeIdx, object] : aSceneTree.mObjectsMap)
    {
        for (const scenic::MeshPart_Naive & part : object.mParts)
        {
            graphics::VertexArrayObject vao = prepareVAO(aProgram, part);
            glBindVertexArray(vao);

            scenic::GenericMaterial_glsl material =
                aEngine.mContext.mStorage.mMaterials.mMaterials[part.mMaterial.mSurfaceParameters.mIndex];

            glBindTextureUnit(diffuseTextureUnit,
                              aEngine.mContext.mStorage.mTextures.at(material.mDiffuseMap.mTextureIndex));

            // TODO: move in the per logic entity buffer
            graphics::setUniform(aProgram, "u_MaterialIdx",
                                 (GLuint)part.mMaterial.mSurfaceParameters.mIndex);

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
        }
        baseInstance += instanceCount;
    }
}


FrameGraph::ProgramStore::ProgramStore(Engine & aEngine) :
    mBlinnPhong{ aEngine.loadProgram(renderer::ReferencePath{ gBlinnPhongProgramPath }) },
    mConeTrace{ aEngine.loadProgram(renderer::ReferencePath{ gConeTraceProgramPath }) },
    mRayTraceVoxels{ aEngine.loadProgram(renderer::ReferencePath{ gRayTraceVoxelsProgramPath }) },
    mVoxelizationProgram{ aEngine.loadProgram(gVoxelizationProgram) },
    mVoxelizationDominantAxisProgram{ aEngine.loadProgram(gVoxelizationDominantAxisProgram) },
    mVoxelizationViewProgram{ aEngine.loadProgram(gVoxelizationViewProgram) },
    mVoxelizationDominantAxisViewProgram{ aEngine.loadProgram(gVoxelizationDominantAxisViewProgram) }

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
    passForward(aSceneTree, mPrograms.mBlinnPhong);
}


void FrameGraph::renderConeTrace(const scenic::SceneTree & aSceneTree,
                                 Voxelizer & aVoxelizer)
{
    const auto & program = mPrograms.mConeTrace;
    glBindTextureUnit(10, aVoxelizer.mOccupancy);

    graphics::setUniform(program, "u_VoxelsAlbedoTexture", 10);
    graphics::setUniform(program, "u_VoxelSize", aVoxelizer.mVoxelSize);
    math::Box<float> aabb = scenic::getAabb(aSceneTree);
    graphics::setUniform(program, "u_AabbMin", aabb.leftBottomZMin());

    graphics::setUniform(program, "u_TanHalfAperture", mFrameControl.mConeAperture.data());
    graphics::setUniform(program, "u_GridAlign", mFrameControl.mGridAlign);


    passForward(aSceneTree, program);
}


void FrameGraph::passForward(const scenic::SceneTree & aSceneTree,
                             const renderer::IntrospectProgram & aProgram)
{
    glPolygonMode(GL_FRONT_AND_BACK, *mFrameControl.mPolygonMode);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    drawPass(aProgram, aSceneTree, mEngine);
}


void FrameGraph::appendUi()
{
    DearImguiWitness witness;

    imguiui::addCombo("Polygon mode",
                      mFrameControl.mPolygonMode,
                      FrameControl::gPolygonModes.begin(),
                      FrameControl::gPolygonModes.end(),
                      [](auto aModeIt) {return graphics::to_string(*aModeIt); });

    ImGui::SliderAngle("Diffuse Cone Aperture", &mFrameControl.mConeAperture.data(), 0.f, 180.f);

    ImGui::Checkbox("Grid Aligned Trace Origin", &mFrameControl.mGridAlign);
}



} // namespace ad