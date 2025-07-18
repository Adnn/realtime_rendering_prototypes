#include "FrameGraph.h"

#include "SetupDrawing.h"
#include "UniformSetterWitness.h"
#include "Voxelization.h"

#include "log/Logging.h"

#include <engine/Lights.h>

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


        const std::filesystem::path gPbrProgramPath = "programs/ch11_RenderModel_Pbr.prog";
        const std::filesystem::path gConeTraceProgramPath = "programs/ch11_ConeTrace.prog";
        const std::filesystem::path gRayTraceVoxelsProgramPath = "programs/ch11_RayTraceVoxels.prog";
        const std::filesystem::path gDebugCubemapProgramPath = "programs/ch11_DebugCubemap.prog";
        const std::filesystem::path gDepthMappingProgramPath = "programs/ch11_DepthMapping.prog";
        const std::filesystem::path gCubeDepthMappingProgramPath = "programs/ch11_CubeDepthMapping.prog";

        const renderer::ReferencePath gVoxelizationProgram{"programs/ch11_Voxelization.prog"};
        const renderer::ReferencePath gVoxelizationDominantAxisProgram{"programs/ch11_VoxelizationDominantAxis.prog"};
        const renderer::ReferencePath gVoxelizationViewProgram{"programs/ch11_View.prog"};
        const renderer::ReferencePath gVoxelizationDominantAxisViewProgram{"programs/ch11_ViewDominantAxis.prog"};

        const renderer::ReferencePath gInjectIrradianceProgramPath{"programs/ch11_InjectIrradiance.prog"};
        const renderer::ReferencePath gFilterIrradianceProgramPath{"programs/ch11_FilterIrradiance.prog"};

        const GLsizei gShadowMapSize = 2048;

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

    const GLint diffuseTextureUnit = 0;
    const GLint normalTextureUnit = 1;
    const GLint mraoTextureUnit = 2;
    graphics::setUniform(aProgram, "u_AlbedoTexture", diffuseTextureUnit);
    graphics::setUniform(aProgram, "u_NormalTexture", normalTextureUnit);
    graphics::setUniform(aProgram, "u_MraoTexture", mraoTextureUnit);

    for (const auto & [nodeIdx, object] : aSceneTree.mObjectsMap)
    {
        for (const scenic::MeshPart_Naive & part : object.mParts)
        {
            graphics::VertexArrayObject vao = prepareVAO(aProgram, part);
            glBindVertexArray(vao);

            scenic::GenericMaterial_glsl material =
                get(aEngine.mContext.mStorage, part.mMaterial.mSurfaceParameters);

            // TODO: handle the no-entry texture (and non-textured stuff in shader)
            if (auto idx = material.mDiffuseMap.mTextureIndex;
                idx != scenic::TextureInput::gNoEntry)
            {
                glBindTextureUnit(diffuseTextureUnit,
                                  aEngine.mContext.mStorage.mTextures.at(idx));
            }
            graphics::setUniform(aProgram, "u_DiffuseUvChannel",
                                 (GLuint)material.mDiffuseMap.mUVAttributeIndex);

            if (auto idx = material.mNormalMap.mTextureIndex;
                idx != scenic::TextureInput::gNoEntry)
            {
                glBindTextureUnit(normalTextureUnit,
                                  aEngine.mContext.mStorage.mTextures.at(idx));
            }
            graphics::setUniform(aProgram, "u_NormalUvChannel",
                                 (GLuint)material.mNormalMap.mUVAttributeIndex);

            if (auto idx = material.mMetallicRoughnessAoMap.mTextureIndex;
                idx != scenic::TextureInput::gNoEntry)
            {
                glBindTextureUnit(mraoTextureUnit,
                                  aEngine.mContext.mStorage.mTextures.at(idx));
            }
            graphics::setUniform(aProgram, "u_MraoUvChannel",
                                 (GLuint)material.mMetallicRoughnessAoMap.mUVAttributeIndex);

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
    mPbr{ aEngine.loadProgram(renderer::ReferencePath{ gPbrProgramPath }) },
    mConeTrace{ aEngine.loadProgram(renderer::ReferencePath{ gConeTraceProgramPath }) },
    mRayTraceVoxels{ aEngine.loadProgram(renderer::ReferencePath{ gRayTraceVoxelsProgramPath }) },
    mDebugCubemap{ aEngine.loadProgram(renderer::ReferencePath{ gDebugCubemapProgramPath }) },
    mDepthMapping{ aEngine.loadProgram(renderer::ReferencePath{ gDepthMappingProgramPath }) },
    mCubeDepthMapping{ aEngine.loadProgram(renderer::ReferencePath{ gCubeDepthMappingProgramPath }) },
    mVoxelizationProgram{ aEngine.loadProgram(gVoxelizationProgram) },
    mVoxelizationDominantAxisProgram{ aEngine.loadProgram(gVoxelizationDominantAxisProgram) },
    mVoxelizationViewProgram{ aEngine.loadProgram(gVoxelizationViewProgram) },
    mVoxelizationDominantAxisViewProgram{ aEngine.loadProgram(gVoxelizationDominantAxisViewProgram) },
    mInjectIrradianceProgram{ aEngine.loadProgram(gInjectIrradianceProgramPath) },
    mFilterIrradianceProgram{ aEngine.loadProgram(gFilterIrradianceProgramPath) }
{}


FrameGraph::FrameGraph(math::Size<2, int> aFrameSize) :
    mPrograms{mEngine},
    mShadowMap{GL_TEXTURE_2D_ARRAY},
    mOmniShadowMap{GL_TEXTURE_CUBE_MAP_ARRAY},
    mIntegratedGgxBrdf{ scenic::integrateEnvironmentBrdf(scenic::gIntegratedBrdfSide, mEngine.mLoader) }
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

    // Shadow map
    {
        // For actual creation
        graphics::ScopedBind{mShadowMap};
        graphics::ScopedBind{mShadowFramebuffer};
    }
    glTextureStorage3D(mShadowMap, 1, GL_DEPTH_COMPONENT24, gShadowMapSize, gShadowMapSize, renderer::gMaxShadowMaps);
    glObjectLabel(GL_TEXTURE, mShadowMap, -1, "shadow_map");
    {
        GLint isSuccess;
        glGetTextureParameteriv(mShadowMap, GL_TEXTURE_IMMUTABLE_FORMAT, &isSuccess);
        assert(isSuccess);
    }

    glTextureParameteri(mShadowMap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(mShadowMap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameterfv(mShadowMap, GL_TEXTURE_BORDER_COLOR,
                         math::hdr::Rgba_f{1.f, 0.f, 0.f, 0.f}.data());
    glTextureParameteri(mShadowMap, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(mShadowMap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTextureParameteri(mShadowMap, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTextureParameteri(mShadowMap, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    // Omni shadow map
    const graphics::Texture & shadowMap = mOmniShadowMap;

    {
        // For actual creation
        graphics::ScopedBind{shadowMap};
    }
    // Important: the depth is the number of layer*faces
    glTextureStorage3D(shadowMap, 1, GL_DEPTH_COMPONENT24, gShadowMapSize, gShadowMapSize, 6 * renderer::gMaxShadowMaps);
    glObjectLabel(GL_TEXTURE, shadowMap, -1, "omni_shadow_map");
    {
        GLint isSuccess;
        glGetTextureParameteriv(shadowMap, GL_TEXTURE_IMMUTABLE_FORMAT, &isSuccess);
        assert(isSuccess);
    }

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glTextureParameteri(shadowMap, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(shadowMap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTextureParameteri(shadowMap, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTextureParameteri(shadowMap, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    // Light view projection UBO
    glBindBufferBase(GL_UNIFORM_BUFFER, 5, mLightViewProjectionUbo);
}


void FrameGraph::resizeFrame(math::Size<2, int> aRenderResolution)
{
}


void FrameGraph::loadPrograms()
{ 
    mPrograms = ProgramStore{mEngine}; 
}


GLfloat toTanHalf(math::Radian<GLfloat> aAngle)
{
    return math::tan(aAngle / 2.f);
}

void FrameGraph::renderFinalScene(const scenic::SceneTree & aSceneTree,
                                  Voxelizer & aVoxelizer)
{
    const auto & program = mPrograms.mPbr;

    glBindTextureUnit(6, mShadowMap);
    graphics::setUniform(program, "u_ShadowMap", 6);
    glBindTextureUnit(7, mOmniShadowMap);
    graphics::setUniform(program, "u_OmniShadowMap", 7);
    glBindTextureUnit(11, aVoxelizer.mIrradiance);
    graphics::setUniform(program, "u_VoxelsIrradianceTexture", 11);

    glBindTextureUnit(15, mIntegratedGgxBrdf);
    graphics::setUniform(program, "u_IntegratedEnvironmentBrdf", 15);

    graphics::setUniform(program, "u_VoxelSize", aVoxelizer.mVoxelSize);
    graphics::setUniform(program, "u_AabbMin", aVoxelizer.mSceneAabb.leftBottomZMin());

    graphics::setUniform(program, "u_ConeMaxDistance", mFrameControl.mConeMaxDistance);
    graphics::setUniform(program, "u_ConeOffsetAlongNormal", mFrameControl.mConeOffsetAlongNormal);
    graphics::setUniform(program, "u_ConeOffsetAlongAxis", mFrameControl.mConeOffsetAlongAxis);
    graphics::setUniform(program, "u_TanHalfAperture", toTanHalf(mFrameControl.mDiffuseConeAperture));
    graphics::setUniform(program, "u_TanHalfShadow", toTanHalf(mFrameControl.mShadowConeAperture));
    graphics::setUniform(program, "u_SpecularConeRoughnessFactor", mFrameControl.mSpecularConeRoughnessFactor);

    graphics::setUniform(program, "u_ToneMapping", (GLuint)mFrameControl.mToneMapping);
    graphics::setUniform(program, "u_ShadowMethod", (GLuint)mFrameControl.mFinalSceneShadow);

    graphics::setUniform(program, "u_ShadowCubeNearDistance", gShadowCubeNearDistance);
    graphics::setUniform(program, "u_ShadowCubeFarDistance", gShadowCubeFarDistance);

    graphics::setUniform(program, "u_SplitSumIndirectSpecular", mFrameControl.mSplitSumIndirectSpecular);

    glProgramUniform4fv(program,
                        glGetUniformLocation(program, "u_LightingFactors"),
                        1, &mFrameControl.mDirectDiffuseFactor);

    passForward(aSceneTree, program);
}


void FrameGraph::renderConeTrace(const scenic::SceneTree & aSceneTree,
                                 Voxelizer & aVoxelizer, GLuint aMode)
{
    const auto & program = mPrograms.mConeTrace;
    glBindTextureUnit(11, aVoxelizer.mIrradiance);
    graphics::setUniform(program, "u_VoxelsIrradianceTexture", 11);
    graphics::setUniform(program, "u_VoxelSize", aVoxelizer.mVoxelSize);
    graphics::setUniform(program, "u_AabbMin", aVoxelizer.mSceneAabb.leftBottomZMin());

    graphics::setUniform(program, "u_ConeMaxDistance", mFrameControl.mConeMaxDistance);
    graphics::setUniform(program, "u_ConeOffsetAlongNormal", mFrameControl.mConeOffsetAlongNormal);
    graphics::setUniform(program, "u_ConeOffsetAlongAxis", mFrameControl.mConeOffsetAlongAxis);
    graphics::setUniform(program, "u_TanHalfAperture", toTanHalf(mFrameControl.mDiffuseConeAperture));
    graphics::setUniform(program, "u_SpecularConeRoughnessFactor", mFrameControl.mSpecularConeRoughnessFactor);
    graphics::setUniform(program, "u_GridAlign", mFrameControl.mGridAlign);

    graphics::setUniform(program, "u_ConeTraceMode", aMode);


    passForward(aSceneTree, program);
}


void FrameGraph::renderDepth(const scenic::SceneTree & aSceneTree, DepthMapType aType)
{
    graphics::ScopedBind boundFbo{mShadowFramebuffer};
    glViewport(0, 0, gShadowMapSize, gShadowMapSize);
    glClear(GL_DEPTH_BUFFER_BIT);

    auto scopePolygonOffset = graphics::scopeFeature(GL_POLYGON_OFFSET_FILL, true);
    glPolygonOffset(mFrameControl.mShadowScaleBias.x(),
                    mFrameControl.mShadowScaleBias.y());

    const auto & program = (aType == DepthMapType::CubeMap) ? 
        mPrograms.mCubeDepthMapping : mPrograms.mDepthMapping;

    if (aType == DepthMapType::CubeMap)
    {
        graphics::setUniform(program, "u_NearDistance", gShadowCubeNearDistance);
        graphics::setUniform(program, "u_FarDistance", gShadowCubeFarDistance);
    }

    passForward(aSceneTree, program);
}


void FrameGraph::renderCubemap(const scenic::SceneTree & aSceneTree)
{
    const auto & program = mPrograms.mDebugCubemap;

    glTextureParameteri(mOmniShadowMap, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    glBindTextureUnit(7, mOmniShadowMap);
    graphics::setUniform(program, "u_CubeMap", 7);

    graphics::setUniform(program, "u_NearDistance", gShadowCubeNearDistance);
    graphics::setUniform(program, "u_FarDistance", gShadowCubeFarDistance);
    passForward(aSceneTree, program);

    glTextureParameteri(mOmniShadowMap, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
}


void FrameGraph::passForward(const scenic::SceneTree & aSceneTree,
                             const renderer::IntrospectProgram & aProgram)
{
    glPolygonMode(GL_FRONT_AND_BACK, *mFrameControl.mPolygonMode);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    // We are alpha-testing in the fragment shader
    // and unless we sort the geometry, alpha blending will likely blend with wrong background
    glDisable(GL_BLEND);
 
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

    imguiui::addComboContinuousEnum<FrameControl::ToneMapping::_End>(
        "Tone Mapping", mFrameControl.mToneMapping);

    ImGui::SliderFloat("Cone max distance", &mFrameControl.mConeMaxDistance, 0.1f, 30.0f);
    ImGui::SliderFloat("Cone normal offset", &mFrameControl.mConeOffsetAlongNormal, 0.f, 5.0f);
    ImGui::SliderFloat("Cone axis offset", &mFrameControl.mConeOffsetAlongAxis, 0.f, 5.0f);
    ImGui::SliderAngle("Diffuse Cone Aperture", &mFrameControl.mDiffuseConeAperture.data(), 1.f, 180.f);
    ImGui::SliderAngle("Shadow Cone Aperture", &mFrameControl.mShadowConeAperture.data(), 1.f, 180.f);
    ImGui::SliderFloat("Specular Cone roughness factor", &mFrameControl.mSpecularConeRoughnessFactor, 0, 2);

    ImGui::Checkbox("Grid Aligned Trace Origin", &mFrameControl.mGridAlign);

    ImGui::Checkbox("Preintegrated BRDF (split-sum) specular indirect", &mFrameControl.mSplitSumIndirectSpecular);

    ImGui::SeparatorText("Lighting factors");
    ImGui::SliderFloat("Direct diffuse", &mFrameControl.mDirectDiffuseFactor, 0.f, 4.f);
    ImGui::SliderFloat("Direct specular", &mFrameControl.mDirectSpecularFactor, 0.f, 4.f);
    ImGui::SliderFloat("Indirect diffuse", &mFrameControl.mIndirectDiffuseFactor, 0.f, 4.f);
    ImGui::SliderFloat("Indirect specular", &mFrameControl.mIndirectSpecularFactor, 0.f, 4.f);

    ImGui::SeparatorText("Shadow");
    imguiui::addComboContinuousEnum<FrameControl::ShadowMethod::_End>(
        "Final Scene Shadow", mFrameControl.mFinalSceneShadow);
    ImGui::InputFloat("Scale", &mFrameControl.mShadowScaleBias.x());
    ImGui::InputFloat("Bias", &mFrameControl.mShadowScaleBias.y());
}


std::string to_string(FrameGraph::FrameControl::ToneMapping aValue)
{
#define STR(enumerator) case FrameGraph::FrameControl::ToneMapping::enumerator: return #enumerator
    switch (aValue)
    {
        STR(None);
        STR(Reinhard);
        STR(Aces);
        STR(AcesApprox);
    default:
        throw std::logic_error{ "Unhandled tone mapping." };
    }
#undef STR
}


std::string to_string(FrameGraph::FrameControl::ShadowMethod aValue)
{
#define STR(enumerator) case FrameGraph::FrameControl::ShadowMethod::enumerator: return #enumerator
    switch (aValue)
    {
        STR(ShadowMap);
        STR(ConeTracing);
    default:
        throw std::logic_error{ "Unhandled shadow method." };
    }
#undef STR
}

} // namespace ad