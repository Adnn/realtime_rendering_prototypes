#include "FrameGraph.h"

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


        const std::filesystem::path gDepthProgramPath = "programs/DepthMap.prog";
        const std::filesystem::path gShowTextureProgramPath = "programs/ShowTexture.prog";
        const std::filesystem::path gSphereSsaoProgramPath = "programs/ch11_global_illumination_SphereSsao.prog";
        const std::filesystem::path gHemisphereSsaoProgramPath = "programs/ch11_global_illumination_HemisphereSsao.prog";
        const std::filesystem::path gBlurTextureProgramPath = "programs/ch11_global_illumination_Blur.prog";
        const std::filesystem::path gForwardPbrProgramPath = "programs/ch11_global_illumination_Pbr.prog";
        const std::filesystem::path gSkyboxProgramPath = "programs/Skybox.prog";

        // Having the texture on the size of the blurring kernel avoids 
        // the noise still showing up after filtering
        constexpr math::Size<2, int> gNoiseResolution{4, 4};
        //constexpr math::Size<2, int> gNoiseResolution{256, 256};


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


DESCRIBE(FrameGraph::SphereSsaoControl)
{
    GIVE_EX(make_Clamped(aValue.mDepthBias, { .mMax = 1.0f }), DepthBias);
    GIVE_EX(make_Clamped(aValue.mSphereRadius, { .mMax = 5.0f }), SphereRadius);
    GIVE(ReflectSamples);
    GIVE(Weighted);
    GIVE_EX(make_Clamped(aValue.mWeightFactor, { .mMax = 50.0f }), WeightFactor);
    GIVE(SphereInScreenSpace);
    GIVE(ScreenSpaceNonLinearDepth);
}

DESCRIBE(FrameGraph::HemiSsaoControl)
{
    GIVE_EX(make_Clamped(aValue.mDepthBias, { .mMax = 1.0f }), DepthBias);
    GIVE_EX(make_Clamped(aValue.mSphereRadius, { .mMax = 5.0f }), SphereRadius);
    GIVE(ImportanceSampling);
    GIVE(RotateSamples);
    GIVE(WeightDistance);
    GIVE(WeightCosine);
    GIVE_EX(make_Clamped(aValue.mDistanceFactor, { .mMax = 50.0f }), DistanceFactor);
}

DESCRIBE(FrameGraph::BlurControl)
{
    GIVE_EX(make_Clamped(aValue.mBlurRadius, {.mMin = 0, .mMax = 32 }), BlurRadius);
    GIVE_EX(make_Clamped(aValue.mDepthFactor, {.mMin = 0, .mMax = 100 }), DepthFactor);
    GIVE_EX(make_Clamped(aValue.mNormalFactor, {.mMin = 0, .mMax = 100 }), NormalFactor);
}

std::string to_string(TextureStore::Name aName)
{
#define STR(enumerator) case TextureStore::##enumerator: return #enumerator
    switch (aName)
    {
        STR(DepthMap);
        STR(FragPositionView);
        STR(FragNormalView);
        STR(RawOcclusion);
        STR(FilteredOcclusion);
    default:
        throw std::logic_error{ "Unhandled TextureStore::Name." };
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


// TODO: this is likely biased toward the corners, implement some heat map to confirm
std::vector<math::Vec<3, GLfloat>> generateUnitSphereSamples_carthesian(unsigned int aCount, Domain aDomain)
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

// TODO: this is likely biased toward the poles, implement some heat map to confirm
std::vector<math::Vec<3, GLfloat>> generateUnitSphereSamples_spherical(unsigned int aCount, Domain aDomain)
{
    std::vector<math::Vec<3, GLfloat>> result;
    result.reserve(aCount);

    std::uniform_real_distribution<GLfloat> azimuthal{ -math::pi<GLfloat>, math::pi<GLfloat> };
    std::uniform_real_distribution<GLfloat> polar{ 0, math::pi<GLfloat> };
    std::uniform_real_distribution<GLfloat> norm{ 0.0f, 1.0f };
    std::default_random_engine e;

    for (unsigned int idx = 0; idx != aCount; ++idx)
    {
        result.push_back(math::Spherical{
            (aDomain == Domain::Volume) ? norm(e) : 1.0f,
            math::Radian<GLfloat>{polar(e)},
            math::Radian<GLfloat>{azimuthal(e)},
        }.toCartesian().as<math::Vec>());
    }

    return result;
}


std::vector<math::Vec<3, GLfloat>> generateHemisphereSample(unsigned int aCount)
{
    std::vector<math::Vec<3, GLfloat>> result;

    std::uniform_real_distribution<GLfloat> xy{ -1.0f, 1.0f };
    std::uniform_real_distribution<GLfloat> z{ 0.0f, 1.0f };
    std::default_random_engine e;

    while(result.size() != aCount)
    {
        math::Vec<3, GLfloat> v{
            xy(e),
            xy(e),
            z(e),
        };

        // Discard the candidates that do not lie inside the unit hemisphere
        if (v.getNorm() > 1.0f)
        {
            continue;
        }

        // TODO: Implement importance sampling at this level

        result.push_back(v);
    }

    return result;
}


std::vector<math::Vec<3, GLfloat>> generateHemisphereSample_importance(unsigned int aCount,
                                                                       bool aWeightDistance,
                                                                       bool aWeightCosine,
                                                                       float aDistanceFactor)
{
    aDistanceFactor = std::max(0.001f, aDistanceFactor);
    const float atanFactor = std::atan(aDistanceFactor);

    std::vector<math::Vec<3, GLfloat>> result;
    result.reserve(aCount);

    std::uniform_real_distribution<GLfloat> polar{ 0, math::pi<GLfloat>/2.0f };
    std::uniform_real_distribution<GLfloat> azimuthal{ -math::pi<GLfloat>, math::pi<GLfloat> };
    std::uniform_real_distribution<GLfloat> uniform{ 0.0f, 1.0f };
    std::default_random_engine e;

    // Note: For importance sampling:
    // * the norm probability is proportional to (1 / (1 + (N * x)^2), x in [0, 1]
    //   * CDF is 1/pi * atan(N * x) + 1/2 -> x = 1/N * tan(u * atan(N))
    // * the polar angle probability is proportional to cos(x), x in [0, pi/2]
    //   * CDF is sin(x) -> x = asin(u)
    // We find the analytical CDF for each, and invert it to get from uniform variable on [0, 1]
    // to the value.
    for (unsigned int idx = 0; idx != aCount; ++idx)
    {
        math::Vec<3, GLfloat> sample = math::Spherical{
            aWeightDistance ?
                std::tan(uniform(e) * atanFactor) / aDistanceFactor
                : uniform(e),
            aWeightCosine ?
                math::Radian<GLfloat>{std::asin(uniform(e))}
                : math::Radian<GLfloat>{polar(e)},
            math::Radian<GLfloat>{azimuthal(e)},
        }.toCartesian().as<math::Vec>();
        
        // Rotate the hemisphere to be centered around Z instead of Y
        auto y = -sample.z();
        sample.z() = sample.y();
        sample.y() = y;
        result.push_back(sample);

        assert(sample.z() >= 0);
    }
    return result;
}


void generateRandomDirections(const graphics::Texture & aDestination, math::Size<2, int> aResolution)
{
    // Using floating point texture, to be able to store negative values without remapping.
    glTextureStorage2D(aDestination, 1, GL_RGB16F, aResolution.width(), aResolution.height());
    glTextureSubImage2D(aDestination, 0, 0, 0, aResolution.width(), aResolution.height(),
                        GL_RGB, GL_FLOAT,
                        generateUnitSphereSamples_spherical(aResolution.area(), Domain::Surface).data());
}


FrameGraph::ProgramStore::ProgramStore(Engine & aEngine) :
    mDepth{ aEngine.loadProgram(renderer::ReferencePath{ gDepthProgramPath },
                                {"OUTPUT_FRAGMENT_VIEW_POSITION",})},
    mShowTexture{ aEngine.loadProgram(renderer::ReferencePath{ gShowTextureProgramPath }) },
    mSphereSsao{ aEngine.loadProgram(renderer::ReferencePath{ gSphereSsaoProgramPath }) },
    mHemisphereSsao{ aEngine.loadProgram(renderer::ReferencePath{ gHemisphereSsaoProgramPath }) },
    mBlurTexture{ aEngine.loadProgram(renderer::ReferencePath{ gBlurTextureProgramPath }) },
    mForwardPbr{ aEngine.loadProgram(renderer::ReferencePath{ gForwardPbrProgramPath }) },
    mSkyboxCubemap{ aEngine.loadProgram(renderer::ReferencePath{ gSkyboxProgramPath }) },
    mSkyboxEquirectangular{ aEngine.loadProgram(renderer::ReferencePath{ gSkyboxProgramPath }, {"EQUIRECTANGULAR",}) }
{}


void TextureStore::setupTexture(Name aName, GLenum aInternalFormat, GLenum aWrapMode)
{
    auto & texture = mStore.at(aName).mTexture;
    glTextureStorage2D(texture,
                       1,
                       aInternalFormat,
                       mScreenTextureSize.width(),
                       mScreenTextureSize.height());

    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, aWrapMode);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, aWrapMode);
}


FrameGraph::FrameGraph(math::Size<2, int> aFrameSize) :
    mTextures{
        .mStore = makeVector(
            TextureStore::Data{makeTexture(GL_TEXTURE_2D, "shadow_map"), TextureStore::LINEARIZE_DEPTH,},
            TextureStore::Data{makeTexture(GL_TEXTURE_2D, "frag_position_view"), TextureStore::DEPTH_FROM_POSITION,},
            TextureStore::Data{makeTexture(GL_TEXTURE_2D, "frag_normal_view"), TextureStore::DIRECTION,},
            TextureStore::Data{makeTexture(GL_TEXTURE_2D, "RawOcclusion"), TextureStore::RAW_RED_CHANNEL,},
            TextureStore::Data{makeTexture(GL_TEXTURE_2D, "FilteredOcclusion"), TextureStore::RAW_RED_CHANNEL,}
        ),
        .mScreenTextureSize{ aFrameSize },
    },
    mNoiseDirections{makeTexture(GL_TEXTURE_2D, "noise_directions")},
    mIntegratedGgxBrdf{ scenic::integrateEnvironmentBrdf(scenic::gIntegratedBrdfSide, mEngine.mLoader) },
    mFinalFrame{makeTexture(GL_TEXTURE_2D, "final_frame")},
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
    glTextureParameteri(tex(TextureStore::DepthMap), GL_TEXTURE_COMPARE_FUNC, GL_LESS);

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

    mTextures.setupTexture(TextureStore::FragNormalView, GL_RGB16F, GL_CLAMP_TO_EDGE);

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

        assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }


    // 
    //
    //
    generateRandomDirections(mNoiseDirections, gNoiseResolution);
    glTextureParameteri(mNoiseDirections, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(mNoiseDirections, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(mNoiseDirections, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(mNoiseDirections, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    
    //
    //
    //
    glTextureStorage2D(mFinalFrame, 1, GL_RGB8, aFrameSize.width(), aFrameSize.height());

    // Dummy VAO
    graphics::ScopedBind{ mDummyVao };
    glObjectLabel(GL_VERTEX_ARRAY, mDummyVao, -1, "dummy_vao");
}


void FrameGraph::loadPrograms()
{ 
    mPrograms = ProgramStore{mEngine}; 
}


void FrameGraph::renderFrame(const scenic::SceneTree& aSceneTree,
                             const scenic::Environment & aEnvironment)
{
    math::Size<2, int> renderResolution = mTextures.mScreenTextureSize;

    // Fragment position pass
    renderFragPosition(aSceneTree);

    graphics::ScopedBind boundFbo{ mFbo, graphics::FrameBufferTarget::Draw };
    glViewport(0, 0, renderResolution.width(), renderResolution.height());
    const GLenum attachmentPerLocation[1] = {
        GL_COLOR_ATTACHMENT0,
    };
    glDrawBuffers(std::size(attachmentPerLocation), attachmentPerLocation);

    // Pass: produce ambient occlusion factors
    glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         tex(TextureStore::RawOcclusion),
                         0);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    // We do not clear the depth buffer, because some AO method can read the closest surface from it.
    // Since we are currently rendering the geometry, we need to pass the depth test when it is equal.
    glDepthFunc(GL_EQUAL);
    switch (mSsaoMethod)
    {
    case FrameGraph::SsaoMethod::Sphere:
        passSphereSsaoFactor(aSceneTree, renderResolution);
        break;
    case FrameGraph::SsaoMethod::OrientedHemishphere:
        passHemisphereSsaoFactor(aSceneTree, renderResolution);
        break;
    }
    glDepthFunc(GL_LESS); // Restore default

    // Pass: filter AO factors
    glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         tex(TextureStore::FilteredOcclusion),
                         0);
    assert(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    passFilterAo(renderResolution);

    // Final frame composition
    glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         mFinalFrame,
                         0);
    glClearColor(0.1f, 0.2f, 0.3f, 1.f); 
    glClear(GL_COLOR_BUFFER_BIT);

    passForwardPbr(aSceneTree, aEnvironment, renderResolution);
    if (mFrameControl.mApplyEnvironment)
    {
        passSkybox(aEnvironment);
    }
}


void FrameGraph::renderFragPosition(const scenic::SceneTree& aSceneTree)
{
    graphics::ScopedBind boundFbo{ mFbo, graphics::FrameBufferTarget::Draw };
    // We reuse the FBO for several passes, changing the color attachment
    glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         tex(TextureStore::FragPositionView),
                         0);

    glFramebufferTexture(GL_DRAW_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT1,
                         tex(TextureStore::FragNormalView),
                         0);

    const GLenum attachmentPerLocation[2] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
    };
    glDrawBuffers(std::size(attachmentPerLocation), attachmentPerLocation);

    glViewport(0, 0, mTextures.mScreenTextureSize.width(), mTextures.mScreenTextureSize.height());
    glClear(GL_DEPTH_BUFFER_BIT);
    glClearTexImage(tex(TextureStore::FragPositionView), 0, GL_RGB, GL_FLOAT, gLowestBorder.data());
    glClearTexImage(tex(TextureStore::FragNormalView), 0, GL_RGB, GL_FLOAT, gLowestBorder.data());

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
void FrameGraph::passSphereSsaoFactor(const scenic::SceneTree& aSceneTree,
                                  math::Size<2, int> aRenderResolution)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    GLint unitIdx = 1;

    glTextureParameteri(tex(TextureStore::DepthMap), GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glBindTextureUnit(unitIdx, tex(TextureStore::DepthMap));
    graphics::setUniform(mPrograms.mSphereSsao, "u_DepthMap", unitIdx);

    glBindTextureUnit(++unitIdx, tex(TextureStore::FragPositionView));
    graphics::setUniform(mPrograms.mSphereSsao, "u_FragPosition_view", unitIdx);

    glBindTextureUnit(++unitIdx, mNoiseDirections);
    graphics::setUniform(mPrograms.mSphereSsao, "u_NoiseDirections", unitIdx);

    // TODO: we actually need the whole viewport, and we could set it once in a uniform buffer
    graphics::setUniform(mPrograms.mSphereSsao, "u_FramebufferSize", aRenderResolution);

    {
        UniformSetterWitness setter{ .mProgram = mPrograms.mSphereSsao.mProgram };
        describe(setter, mSphereSsaoControl);
    }

    // TODO: load once, in a uniform buffer
    for (unsigned int i = 0; i != gSsaoSampleCount; ++i)
    {
        graphics::setUniform(mPrograms.mSphereSsao,
                             "u_SsaoSamples[" + std::to_string(i) + "]",
                             mSphereSamples[i]);
    }
    
    drawPass(mPrograms.mSphereSsao, aSceneTree);
}


void FrameGraph::passHemisphereSsaoFactor(const scenic::SceneTree& aSceneTree,
                                         math::Size<2, int> aRenderResolution)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    GLint unitIdx = 1;

    glBindTextureUnit(++unitIdx, tex(TextureStore::FragPositionView));
    graphics::setUniform(mPrograms.mHemisphereSsao, "u_FragPosition_view", unitIdx);

    glBindTextureUnit(++unitIdx, tex(TextureStore::FragNormalView));
    graphics::setUniform(mPrograms.mHemisphereSsao, "u_FragNormal_view", unitIdx);

    // TODO: we should ideally use a noise in the XY plane for this situation
    glBindTextureUnit(++unitIdx, mNoiseDirections);
    graphics::setUniform(mPrograms.mHemisphereSsao, "u_NoiseDirections", unitIdx);

    // TODO: we actually need the whole viewport, and we could set it once in a uniform buffer
    graphics::setUniform(mPrograms.mHemisphereSsao, "u_FramebufferSize", aRenderResolution);

    {
        UniformSetterWitness setter{ .mProgram = mPrograms.mHemisphereSsao.mProgram };
        describe(setter, mHemisphereSsaoControl);
    }

    // Regenerate, in case the controls changed
    mHemisphereSamples_importance = 
        generateHemisphereSample_importance(gSsaoSampleCount,
                                            mHemisphereSsaoControl.mWeightDistance,
                                            mHemisphereSsaoControl.mWeightCosine,
                                            mHemisphereSsaoControl.mDistanceFactor);

    // TODO: load once, in a uniform buffer
    for (unsigned int i = 0; i != gSsaoSampleCount; ++i)
    {
        graphics::setUniform(mPrograms.mHemisphereSsao,
                             "u_SsaoSamples[" + std::to_string(i) + "]",
                             (mHemisphereSsaoControl.mImportanceSampling ?
                                mHemisphereSamples_importance[i]
                                : mHemisphereSamples[i]));
    }
    
    drawPass(mPrograms.mHemisphereSsao, aSceneTree);
}

void FrameGraph::passFilterAo(math::Size<2, int> aRenderResolution)
{
    glDisable(GL_DEPTH_TEST);

    GLint unitIdx = 1;
    glBindTextureUnit(unitIdx, tex(TextureStore::RawOcclusion));
    graphics::setUniform(mPrograms.mBlurTexture, "u_Texture", unitIdx);

    glBindTextureUnit(++unitIdx, tex(TextureStore::FragPositionView));
    graphics::setUniform(mPrograms.mBlurTexture, "u_FragPosition_view", unitIdx);

    glBindTextureUnit(++unitIdx, tex(TextureStore::FragNormalView));
    graphics::setUniform(mPrograms.mBlurTexture, "u_FragNormal_view", unitIdx);

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

void FrameGraph::passForwardPbr(const scenic::SceneTree & aSceneTree,
                                const scenic::Environment & aEnvironment,
                                math::Size<2, int> aRenderResolution)
{
    // When rendering the point or wireframe, we have to discard the existing depth buffer
    // because users do not expect "filled-faces occlusion" in such situtations.
    if (*mFrameControl.mPolygonMode != GL_FILL)
    {
        glDepthFunc(GL_LESS);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
    // When rendering filled triangles, we can reuse the existing depth buffer, and discard
    // every fragment that does not exactly match.
    else
    {
        glDepthFunc(GL_EQUAL);
    }

    glPolygonMode(GL_FRONT_AND_BACK, *mFrameControl.mPolygonMode);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    const auto& program = mPrograms.mForwardPbr;

    GLint unitIdx = 1;
    glBindTextureUnit(unitIdx, tex(TextureStore::FilteredOcclusion));
    graphics::setUniform(program, "u_AmbientOcclusion", unitIdx);

    ++unitIdx;
    glBindTextureUnit(unitIdx, aEnvironment.mIrradianceMap.mTexture);
    graphics::setUniform(program, "u_FilteredIrradianceEnvironmentTexture", unitIdx);

    ++unitIdx;
    glBindTextureUnit(unitIdx, aEnvironment.mGgxRadianceMap.mTexture);
    graphics::setUniform(program, "u_FilteredRadianceEnvironmentTexture", unitIdx);

    ++unitIdx;
    glBindTextureUnit(unitIdx, mIntegratedGgxBrdf);
    graphics::setUniform(program, "u_IntegratedEnvironmentBrdf", unitIdx);

    // TODO: we actually need the whole viewport, and we could set it once in a uniform buffer
    graphics::setUniform(program, "u_FramebufferSize", aRenderResolution);

    graphics::setUniform(program, "u_ApplyAo", mFrameControl.mApplyAo);
    graphics::setUniform(program, "u_ApplyEnvironment", mFrameControl.mApplyEnvironment);

    graphics::setUniform(program, "u_SpecularIblFactor", mFrameControl.mSpecularIblFactor);
    graphics::setUniform(program, "u_DiffuseIblFactor", mFrameControl.mDiffuseIblFactor);

    drawPass(program, aSceneTree);

    glDepthFunc(GL_LESS); // Restore default
}


void FrameGraph::passSkybox(const scenic::Environment & aEnvironment)
{
    const scenic::EnvironmentMap & shownEnv = aEnvironment.get(mFrameControl.mSkyboxCategory);

    const auto & program = shownEnv.isCubemap() ?
        mPrograms.mSkyboxCubemap : mPrograms.mSkyboxEquirectangular;

    graphics::setUniform(program, "u_LodBias", mFrameControl.mSkyboxLodBias);
    return scenic::passSkyboxBase(program,
                                  shownEnv,
                                  GL_FRONT, // Rendering from inside the skybox
                                  *mFrameControl.mPolygonMode);
}


void FrameGraph::passShowNoise()
{
    glDisable(GL_DEPTH_TEST);

    graphics::setUniform(mPrograms.mShowTexture, "u_Mode", TextureStore::Mode::DIRECTION);

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

    imguiui::addCombo("Polygon mode",
                      mFrameControl.mPolygonMode,
                      FrameControl::gPolygonModes.begin(),
                      FrameControl::gPolygonModes.end(),
                      [](auto aModeIt) {return graphics::to_string(*aModeIt); });

    ImGui::Checkbox("Apply AO", &mFrameControl.mApplyAo);

    ImGui::Checkbox("Apply environment", &mFrameControl.mApplyEnvironment);
    imguiui::addComboContinuousEnum<scenic::Environment::_End>("Skybox category",
                                                               mFrameControl.mSkyboxCategory);
    // TODO: we might query the currently selected texture max LOD for the upper limit
    ImGui::SliderFloat("Skybox bias", &mFrameControl.mSkyboxLodBias, 0.f, 8.f);

    ImGui::SliderFloat("SpecularIblFactor", &mFrameControl.mSpecularIblFactor, 0.f, 2.f);
    ImGui::SliderFloat("DiffuseIblFactor", &mFrameControl.mDiffuseIblFactor, 0.f, 2.f);

    ImGui::SeparatorText("SSAO");
    imguiui::addComboContinuousEnum<SsaoMethod::_End>("SSAO Method", mSsaoMethod);

    switch (mSsaoMethod)
    {
    case FrameGraph::SsaoMethod::Sphere:
        describe(witness, mSphereSsaoControl);
        break;
    case FrameGraph::SsaoMethod::OrientedHemishphere:
        describe(witness, mHemisphereSsaoControl);
        break;
    }

    ImGui::SeparatorText("AO Blur");
    describe(witness, mBlurControl);
}


} // namespace ad