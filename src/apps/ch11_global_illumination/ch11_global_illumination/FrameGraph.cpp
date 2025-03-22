#include "FrameGraph.h"

#include "SetupDrawing.h"
#include "UniformSetterWitness.h"

#include <handy/vector_utils.h>

#include <reflect/DearImguiWitness.h>
#include <reflect/ReflectHelpers.h>

#include <renderer/Uniforms.h>

#include <scenic/Camera.h>

#include <random>


namespace ad {

    namespace {


        const std::filesystem::path gDepthProgramPath = "programs/DepthMap.prog";
        const std::filesystem::path gShowTextureProgramPath = "programs/ShowTexture.prog";
        const std::filesystem::path gShowSsaoProgramPath = "programs/ch11_global_illumination_Ssao.prog";
        const std::filesystem::path gBlurTextureProgramPath = "programs/ch11_global_illumination_Blur.prog";


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


    static constexpr std::array<GLfloat, 4> gLowestBorder = []() -> std::array<GLfloat, 4>
        {
            GLfloat lowest = std::numeric_limits<GLfloat>::lowest();
            return { lowest, lowest, lowest, lowest };
        }();

    } // unnamed namespace


DESCRIBE(FrameGraph::SsaoControl)
{
    GIVE_EX(make_Clamped(aValue.mDepthBias, { .mMax = 1.0f }), DepthBias);
    GIVE_EX(make_Clamped(aValue.mSphereRadius, { .mMax = 5.0f }), SphereRadius);
    GIVE(ReflectSamples);
    GIVE(Weighted);
    GIVE_EX(make_Clamped(aValue.mWeightFactor, { .mMax = 50.0f }), WeightFactor);
    GIVE(SphereInScreenSpace);
}

DESCRIBE(FrameGraph::BlurControl)
{
    GIVE_EX(make_Clamped(aValue.mBlurRadius, {.mMin = 0, .mMax = 32 }), BlurRadius);
}

std::string to_string(TextureStore::Name aName)
{
#define STR(enumerator) case TextureStore::##enumerator: return #enumerator
    switch (aName)
    {
        STR(DepthMap);
        STR(FragPositionView);
        STR(RawOcclusion);
        STR(FilteredOcclusion);
    }
#undef STR
}

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


// TODO: this is likely biased toward the corners, make an unbiased distribution.
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
        // TODO: importance sample to implement the "quadratic attenuation"
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
    mDepth{ aEngine.loadProgram(renderer::ReferencePath{ gDepthProgramPath },
                                {"OUTPUT_FRAGMENT_VIEW_POSITION",})},
    mShowTexture{ aEngine.loadProgram(renderer::ReferencePath{ gShowTextureProgramPath }) },
    mShowSsao{ aEngine.loadProgram(renderer::ReferencePath{ gShowSsaoProgramPath }) },
    mBlurTexture{ aEngine.loadProgram(renderer::ReferencePath{ gBlurTextureProgramPath }) }
{}


FrameGraph::FrameGraph(math::Size<2, int> aFrameSize) :
    mTextures{
        .mStore = makeVector(
            TextureStore::Data{makeTexture(GL_TEXTURE_2D, "shadow_map"), TextureStore::LINEARIZE_DEPTH,},
            TextureStore::Data{makeTexture(GL_TEXTURE_2D, "frag_position_view"), TextureStore::DEPTH_FROM_POSITION,},
            TextureStore::Data{makeTexture(GL_TEXTURE_2D, "RawOcclusion"), TextureStore::RAW_RED_CHANNEL,},
            TextureStore::Data{makeTexture(GL_TEXTURE_2D, "FilteredOcclusion"), TextureStore::RAW_RED_CHANNEL,}
        ),
        .mScreenTextureSize{ aFrameSize },
    },
    mNoiseDirections{makeTexture(GL_TEXTURE_2D, "noise_directions")},
    mPrograms{mEngine}
{
    //
    //
    //
    glTextureStorage2D(tex(TextureStore::DepthMap),
                       1,
                       GL_DEPTH_COMPONENT24,
                       mTextures.mScreenTextureSize.width(),
                       mTextures.mScreenTextureSize.height());

    // Set texture comparison mode, allowing to compare the depth component to a reference value
    glTextureParameteri(tex(TextureStore::DepthMap), GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTextureParameteri(tex(TextureStore::DepthMap), GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glTextureParameteri(tex(TextureStore::DepthMap), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(tex(TextureStore::DepthMap), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameterfv(tex(TextureStore::DepthMap), GL_TEXTURE_BORDER_COLOR,
                         math::hdr::Rgba_f{1.f, 0.f, 0.f, 0.f}.data());

    // We disable mipmap minification filter, otherwise a mutable texture
    // would not be mipmap complete, and could not be sampled.
    // (will be touched again by the PCF parameter)
    glTextureParameteri(tex(TextureStore::DepthMap), GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // 
    //
    //
    glTextureStorage2D(tex(TextureStore::FragPositionView),
                       1,
                       GL_RGB16F,
                       mTextures.mScreenTextureSize.width(),
                       mTextures.mScreenTextureSize.height());

    glTextureParameteri(tex(TextureStore::FragPositionView), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex(TextureStore::FragPositionView), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 
    //
    //
    {
        auto & texture = tex(TextureStore::RawOcclusion);
        glTextureStorage2D(texture,
                           1,
                           // TODO: should we just use 8-bit normalized integer?
                           GL_R16F,
                           mTextures.mScreenTextureSize.width(),
                           mTextures.mScreenTextureSize.height());

        glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    {
        auto & texture = tex(TextureStore::FilteredOcclusion);
        glTextureStorage2D(texture,
                           1,
                           // TODO: should we just use 8-bit normalized integer?
                           GL_R16F,
                           mTextures.mScreenTextureSize.width(),
                           mTextures.mScreenTextureSize.height());

        glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    //
    // FBO attachments
    //
    {
        graphics::ScopedBind boundFbo{mFbo, graphics::FrameBufferTarget::Draw};
        // The texture attachment is permanent, no need to recreate it each time the FBO is bound
        glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
                             GL_DEPTH_ATTACHMENT,
                             tex(TextureStore::DepthMap),
                             /*mip map level*/0);

        const GLenum attachmentPerLocation[1] = {
            GL_COLOR_ATTACHMENT0,
        };
        assert(std::size(attachmentPerLocation) == 1); // Remove that when we extend
        glDrawBuffers(std::size(attachmentPerLocation), attachmentPerLocation);

        assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }


    // 
    //
    //
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


void FrameGraph::renderFrame(const scenic::SceneTree& aSceneTree,
                             math::Size<2, int> aRenderResolution)
{
    // Fragment position pass
    renderFragPosition(aSceneTree);

    graphics::ScopedBind boundFbo{ mFbo, graphics::FrameBufferTarget::Draw };
    glViewport(0, 0, mTextures.mScreenTextureSize.width(), mTextures.mScreenTextureSize.height());

    // Pass: produce ambient occlusion factors
    glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         tex(TextureStore::RawOcclusion),
                         0);

    // TODO: DO NOT CLEAR THE DEPTH BUFFER, once we render the SSAO factor pass without rendering
    // geometry
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    passSsaoFactor(aSceneTree, aRenderResolution);

    // Pass: filter AO factors
    glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         tex(TextureStore::FilteredOcclusion),
                         0);
    assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    passFilterAo(aRenderResolution);
}


void FrameGraph::renderFragPosition(const scenic::SceneTree& aSceneTree)
{
    graphics::ScopedBind boundFbo{ mFbo, graphics::FrameBufferTarget::Draw };
    // We reuse the FBO for several passes, changing the color attachment
    glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         tex(TextureStore::FragPositionView),
                         0);

    glViewport(0, 0, mTextures.mScreenTextureSize.width(), mTextures.mScreenTextureSize.height());
    glClear(GL_DEPTH_BUFFER_BIT);
    glClearTexImage(tex(TextureStore::FragPositionView), 0, GL_RGBA, GL_FLOAT, gLowestBorder.data());

    passFragPosition(aSceneTree);
}


void FrameGraph::passFragPosition(const scenic::SceneTree & aSceneTree)
{

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    // A requirement if we want to write to a vec3 color output 
    // otherwise, it seems the alpha is treated as being zero, and nothing is actually written
    glDisable(GL_BLEND);

    drawPass(mPrograms.mDepth, aSceneTree);
}


// TODO: we could avoid the vertex processing stage here, everything in the frag position texture
void FrameGraph::passSsaoFactor(const scenic::SceneTree& aSceneTree,
                                  math::Size<2, int> aRenderResolution)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    GLint unitIdx = 1;

    glTextureParameteri(tex(TextureStore::DepthMap), GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glBindTextureUnit(unitIdx, tex(TextureStore::DepthMap));
    graphics::setUniform(mPrograms.mShowSsao, "u_DepthMap", unitIdx);

    glBindTextureUnit(++unitIdx, tex(TextureStore::FragPositionView));
    graphics::setUniform(mPrograms.mShowSsao, "u_FragPosition_view", unitIdx);

    glBindTextureUnit(++unitIdx, mNoiseDirections);
    graphics::setUniform(mPrograms.mShowSsao, "u_NoiseDirections", unitIdx);

    // TODO: we actually need the whole viewport, and we could set it once in a uniform buffer
    graphics::setUniform(mPrograms.mShowSsao, "u_FramebufferSize", aRenderResolution);

    {
        UniformSetterWitness setter{ .mProgram = mPrograms.mShowSsao.mProgram };
        describe(setter, mSsaoControl);
    }

    // TODO: load once, in a uniform buffer
    for (unsigned int i = 0; i != gSsaoSampleCount; ++i)
    {
        graphics::setUniform(mPrograms.mShowSsao,
                             "u_SsaoSamples[" + std::to_string(i) + "]",
                             mSsaoSamples[i]);
    }
    
    drawPass(mPrograms.mShowSsao, aSceneTree);
}

void FrameGraph::passFilterAo(math::Size<2, int> aRenderResolution)
{
    glDisable(GL_DEPTH_TEST);

    const GLint unitIdx = 1;
    glBindTextureUnit(unitIdx, tex(TextureStore::RawOcclusion));
    graphics::setUniform(mPrograms.mBlurTexture, "u_Texture", unitIdx);

    // TODO: we actually need the whole viewport, and we could set it once in a uniform buffer
    graphics::setUniform(mPrograms.mBlurTexture, "u_FramebufferSize", aRenderResolution);

    {
        UniformSetterWitness setter{ .mProgram = mPrograms.mBlurTexture.mProgram };
        describe(setter, mBlurControl);
    }

    glUseProgram(mPrograms.mBlurTexture);
    glBindVertexArray(mDummyVao);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}



#define MODE_LINEARIZE_DEPTH 1u
#define MODE_DIRECTION 2u
#define MODE_DEPTH_FROM_POSITION 3u

void FrameGraph::passShowDepth(const scenic::Camera & aCamera)
{
    glDisable(GL_DEPTH_TEST);

    graphics::setUniform(mPrograms.mShowTexture, "u_Mode", MODE_LINEARIZE_DEPTH);

    glTextureParameteri(tex(TextureStore::DepthMap), GL_TEXTURE_COMPARE_MODE, GL_NONE);
    const GLint unitIdx = 1;
    glBindTextureUnit(unitIdx, tex(TextureStore::DepthMap));
    graphics::setUniform(mPrograms.mShowTexture, "u_Texture", unitIdx);

    auto [near, far] = scenic::getNearFarPlanes(aCamera);
    graphics::setUniform(mPrograms.mShowTexture, "u_NearDistance", near);
    graphics::setUniform(mPrograms.mShowTexture, "u_FarDistance", far);
    graphics::setUniform(mPrograms.mShowTexture, "u_Mode", MODE_LINEARIZE_DEPTH);

    glUseProgram(mPrograms.mShowTexture);
    glBindVertexArray(mDummyVao);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


void FrameGraph::passShowLinearDepth(const scenic::Camera & aCamera)
{
    glDisable(GL_DEPTH_TEST);

    graphics::setUniform(mPrograms.mShowTexture, "u_Mode", MODE_DEPTH_FROM_POSITION);

    const GLint unitIdx = 1;
    glBindTextureUnit(unitIdx, tex(TextureStore::FragPositionView));
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

    graphics::setUniform(mPrograms.mShowTexture, "u_Mode", MODE_DIRECTION);

    const GLint unitIdx = 1;
    glBindTextureUnit(unitIdx, mNoiseDirections);
    graphics::setUniform(mPrograms.mShowTexture, "u_Texture", unitIdx);

    glUseProgram(mPrograms.mShowTexture);
    glBindVertexArray(mDummyVao);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


void FrameGraph::passShowTexture(const scenic::Camera & aCamera,
                                 TextureStore::Name aName)
{
    glDisable(GL_DEPTH_TEST);

    graphics::setUniform(mPrograms.mShowTexture, "u_Mode", mTextures.mStore.at(aName).mMode);

    // In case this is the depth map:
    glTextureParameteri(tex(aName), GL_TEXTURE_COMPARE_MODE, GL_NONE);

    const GLint unitIdx = 1;
    glBindTextureUnit(unitIdx, tex(aName));
    graphics::setUniform(mPrograms.mShowTexture, "u_Texture", unitIdx);

    auto [near, far] = scenic::getNearFarPlanes(aCamera);
    graphics::setUniform(mPrograms.mShowTexture, "u_NearDistance", near);
    graphics::setUniform(mPrograms.mShowTexture, "u_FarDistance", far);

    glUseProgram(mPrograms.mShowTexture);
    glBindVertexArray(mDummyVao);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}



void FrameGraph::appendUi()
{
    DearImguiWitness witness;
    describe(witness, mSsaoControl);
    describe(witness, mBlurControl);
}


} // namespace ad