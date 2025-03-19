#include "FrameGraph.h"

#include "SetupDrawing.h"


namespace ad {

    namespace {


        const std::filesystem::path gDepthProgramPath = "programs/DepthMap.prog";


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


FrameGraph::FrameGraph(math::Size<2, int> aFrameSize) :
    mShadowMap{makeTexture(GL_TEXTURE_2D, "shadow_map")},
    mDepthProgram{ mEngine.loadProgram(renderer::ReferencePath{gDepthProgramPath}) }
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
}


void FrameGraph::loadPrograms()
{
    mDepthProgram =
        mEngine.loadProgram(renderer::ReferencePath{ gDepthProgramPath });
}


void FrameGraph::passDepth(const scenic::SceneTree & aSceneTree)
{
    graphics::ScopedBind boundFbo{mFbo, graphics::FrameBufferTarget::Draw};
    glViewport(0, 0, mShadowMapSize.width(), mShadowMapSize.height());
    glClear(GL_DEPTH_BUFFER_BIT);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    drawPass(mDepthProgram, aSceneTree);
}

} // namespace ad