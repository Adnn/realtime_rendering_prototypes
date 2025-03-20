#include "FrameGraph.h"

#include "SetupDrawing.h"

#include <renderer/Uniforms.h>

#include <scenic/Camera.h>

#include <random>


namespace ad {

    namespace {


        const std::filesystem::path gDepthProgramPath = "programs/DepthMap.prog";
        const std::filesystem::path gShowTextureProgramPath = "programs/ShowTexture.prog";
        const std::filesystem::path gShowSsaoProgramPath = "programs/ch11_global_illumination_Ssao.prog";


        graphics::Texture makeTexture(GLenum aTarget, const char * aDebugName)
        {
            // TODO: replace with glCreateTextures
            graphics::Texture texture(aTarget);

            {
                graphics::ScopedBind dummyBindToCreate{ texture };
            }

            // Doesn not work before the object is actually created.
            glObjectLabel(GL_TEXTURE, texture, -1, aDebugName);
            return texture;
        }


    } // unnamed namespace


void drawPass(const renderer::IntrospectProgram & aProgram, 
              const scenic::SceneTree & aSceneTree)
{
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


std::vector<math::Vec<3, GLfloat>> generateUnitSphereSamples(unsigned int aCount, Domain aDomain)
{
    std::vector<math::Vec<3, GLfloat>> result;
    result.reserve(aCount);

    std::uniform_real_distribution<GLfloat> coord{ -1.0f, 1.0f };
    std::uniform_real_distribution<GLfloat> norm{ 0.0f, 1.0f };
    std::default_random_engine e;

    for (unsigned int idx = 0; idx != aCount; ++idx)
    {
        math::Vec<3, GLfloat> v{
            coord(e),
            coord(e),
            coord(e),
        };
        // TODO: importance sample to implement the "quadratic attenation"
        // see: https://iquilezles.org/articles/ssao/
        switch (aDomain)
        {
        case Domain::Surface:
            result.push_back(v.normalize());
            break;
        case Domain::Volume:
            result.push_back(v.normalize() * norm(e));
            break;
        }
    }

    return result;
}


void generateRandomDirections(const graphics::Texture & aDestination, math::Size<2, int> aResolution)
{
    // Using floating point texture, to be able to store negative values without remapping.
    glTextureStorage2D(aDestination, 1, GL_RGB16F, aResolution.width(), aResolution.height());
    glTextureSubImage2D(aDestination, 0, 0, 0, aResolution.width(), aResolution.height(),
                        GL_RGB, GL_FLOAT,
                        generateUnitSphereSamples(aResolution.area(), Domain::Surface).data());
}

FrameGraph::ProgramStore::ProgramStore(Engine & aEngine) :
    mDepth{ aEngine.loadProgram(renderer::ReferencePath{ gDepthProgramPath }) },
    mShowTexture{ aEngine.loadProgram(renderer::ReferencePath{ gShowTextureProgramPath }) },
    mShowSsao{ aEngine.loadProgram(renderer::ReferencePath{ gShowSsaoProgramPath }) }
{}


FrameGraph::FrameGraph(math::Size<2, int> aFrameSize) :
    mShadowMap{makeTexture(GL_TEXTURE_2D, "shadow_map")},
    mNoiseDirections{makeTexture(GL_TEXTURE_2D, "noise_directions")},
    mPrograms{mEngine}
{
    mShadowMapSize = aFrameSize;
    glTextureStorage2D(mShadowMap,
                       1,
                       GL_DEPTH_COMPONENT24,
                       mShadowMapSize.width(),
                       mShadowMapSize.height());

    // Set texture comparison mode, allowing to compare the depth component to a reference value
    glTextureParameteri(mShadowMap, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTextureParameteri(mShadowMap, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glTextureParameteri(mShadowMap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(mShadowMap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameterfv(mShadowMap, GL_TEXTURE_BORDER_COLOR,
                         math::hdr::Rgba_f{1.f, 0.f, 0.f, 0.f}.data());

    // We disable mipmap minification filter, otherwise a mutable texture
    // would not be mipmap complete, and could not be sampled.
    // (will be touched again by the PCF parameter)
    glTextureParameteri(mShadowMap, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    {
        graphics::ScopedBind boundFbo{mFbo, graphics::FrameBufferTarget::Draw};
        // The texture attachment is permanent, no need to recreate it each time the FBO is bound
        glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
                             GL_DEPTH_ATTACHMENT,
                             mShadowMap,
                             /*mip map level*/0);
        assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }

    generateRandomDirections(mNoiseDirections, { 64, 64 });
    glTextureParameteri(mNoiseDirections, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(mNoiseDirections, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(mNoiseDirections, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(mNoiseDirections, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Dummy VAO
    graphics::ScopedBind{ mDummyVao };
    glObjectLabel(GL_VERTEX_ARRAY, mDummyVao, -1, "dummy_vao");
}


void FrameGraph::loadPrograms()
{ 
    mPrograms = ProgramStore{mEngine}; 
}


void FrameGraph::renderDepth(const scenic::SceneTree& aSceneTree)
{
    graphics::ScopedBind boundFbo{ mFbo, graphics::FrameBufferTarget::Draw };
    glViewport(0, 0, mShadowMapSize.width(), mShadowMapSize.height());
    glClear(GL_DEPTH_BUFFER_BIT);

    passDepth(aSceneTree);
}


void FrameGraph::passDepth(const scenic::SceneTree & aSceneTree)
{

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    drawPass(mPrograms.mDepth, aSceneTree);
}


void FrameGraph::renderSsaoFactor(const scenic::SceneTree& aSceneTree,
                                  math::Size<2, int> aRenderResolution)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    GLint unitIdx = 1;

    glTextureParameteri(mShadowMap, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glBindTextureUnit(unitIdx, mShadowMap);
    graphics::setUniform(mPrograms.mShowSsao, "u_DepthMap", unitIdx);

    glBindTextureUnit(++unitIdx, mNoiseDirections);
    graphics::setUniform(mPrograms.mShowSsao, "u_NoiseDirections", unitIdx);

    graphics::setUniform(mPrograms.mShowSsao, "u_FramebufferSize", aRenderResolution);

    // TODO: load once, in a uniform buffer
    for (unsigned int i = 0; i != gSsaoSampleCount; ++i)
    {
        graphics::setUniform(mPrograms.mShowSsao,
                             "u_SsaoSamples[" + std::to_string(i) + "]",
                             mSsaoSamples[i]);
    }
    
    drawPass(mPrograms.mShowSsao, aSceneTree);
}


void FrameGraph::passShowDepth(const scenic::Camera & aCamera)
{
    glDisable(GL_DEPTH_TEST);

    glTextureParameteri(mShadowMap, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    const GLint unitIdx = 1;
    glBindTextureUnit(unitIdx, mShadowMap);
    graphics::setUniform(mPrograms.mShowTexture, "u_Texture", unitIdx);

    auto [near, far] = scenic::getNearFarPlanes(aCamera);
    graphics::setUniform(mPrograms.mShowTexture, "u_NearDistance", near);
    graphics::setUniform(mPrograms.mShowTexture, "u_FarDistance", far);

    glUseProgram(mPrograms.mShowTexture);
    glBindVertexArray(mDummyVao);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


void FrameGraph::passShowNoise()
{
    glDisable(GL_DEPTH_TEST);

    const GLint unitIdx = 1;
    glBindTextureUnit(unitIdx, mNoiseDirections);
    graphics::setUniform(mPrograms.mShowTexture, "u_Texture", unitIdx);

    glUseProgram(mPrograms.mShowTexture);
    glBindVertexArray(mDummyVao);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


} // namespace ad