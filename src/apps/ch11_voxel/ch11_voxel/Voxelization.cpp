#include "Voxelization.h"

#include "Engine.h"
#include "FrameGraph.h"

#include <scenic/Camera.h>
#include <scenic/Model.h>

#include <renderer/BufferBase.h>
#include <renderer/Uniforms.h>
#include <renderer/BufferLoad.h>


namespace ad {

namespace {

constexpr GLint gAlbedoImageUnit = 0;
constexpr GLint gNormalImageUnit = 1;
// Must be able to co-exist with 6 aniso mipmap images
constexpr GLint gIrradianceImageUnit = 6;
// Must be less than 3, because we might map 6 aniso images
// and max image units might be as low as 8
constexpr GLint gIrradianceMipmapImageUnit = 0;
// TODO: synchronize with compute shader
constexpr math::Vec<3, GLuint> gWorkgroupSize{8u, 8u, 8u};
constexpr GLenum gIrradianceFormat = GL_RGBA8;

scenic::Camera prepareVoxelizationCamera(math::Box<float> aAabb)
{
    const float maxSide = *aAabb.mDimension.getMaxMagnitudeElement();

    scenic::Camera camera;
    // The camera is looking down -Z, as per our convention.
    // To align the zero-texel to the far plane, we have to move a distance of maxSide
    // along the Z-axis 
    // (which might be further than ZMax, if depth() is not the largest dimension)
    // Note: the remapping from [-Far, 0] to [0, GridDimension - 1] happens in the shader
    math::Position<3, GLfloat> position =
        aAabb.leftBottomZMin()
        + math::Vec<3, GLfloat>{maxSide / 2.f, maxSide / 2.f, maxSide};
    camera.setPose(math::trans3d::translate(-position.as<math::Vec>()));
    camera.setupOrthographicProjection({
        .mAspectRatio = 1,
        .mViewHeight = maxSide,
        .mNearZ = 0,
        .mFarZ = -maxSide, // right-handed basis 
    });
    return camera;
}


graphics::Texture prepare3dTexture(GLenum aImageFormat,
                                   GLuint aGridDimension,
                                   GLsizei aLevels,
                                   const char * aName)
{
    graphics::Texture texture{GL_TEXTURE_3D};
    // For creation
    graphics::bind(texture);
    glObjectLabel(GL_TEXTURE, texture, -1, aName);

    // non-normalized integer texture should not use filtering
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTextureStorage3D(texture, aLevels, aImageFormat,
                       aGridDimension, aGridDimension, aGridDimension);

    return texture;
}

} // unnamed namespace


std::size_t VoxelsSsbo_glsl::ComputeByteSize(GLuint aGridDimension)
{
    const unsigned int voxelCount = std::pow(aGridDimension, 3);
    return offsetof(VoxelsSsbo_glsl, mVoxels) 
           + voxelCount * sizeof(std::uint8_t);
}


Voxelizer::Voxelizer()
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mVoxelStore); // Generate the buffer
    glObjectLabel(GL_BUFFER, mVoxelStore, -1, "ssbo_voxel_store");

    glBindBuffer(mVoxelizationViewBuffer.GLTarget_v, mVoxelizationViewBuffer); // For creation
    glObjectLabel(GL_BUFFER, mVoxelizationViewBuffer, -1, "VoxelizationViewProjection");
}


Guard Voxelizer::guardConservativeRasterization()
{
    if (mControl.mConservativeRasterization
        && GLAD_GL_NV_conservative_raster)
    {
        glEnable(GL_CONSERVATIVE_RASTERIZATION_NV);
        return Guard{[]() { glDisable(GL_CONSERVATIVE_RASTERIZATION_NV); }};
    }
    else
    {
        return Guard{[]() {}};
    }
}

void Voxelizer::recordSceneAabb(const scenic::SceneTree & aScene, GLuint aGridDimension)
{
    mSceneAabb = scenic::getAabb(aScene);
    const float maxSide = *mSceneAabb.mDimension.getMaxMagnitudeElement();
    mVoxelSize = maxSide / aGridDimension;
}


void Voxelizer::voxelizeDominantAxis(const scenic::SceneTree & aScene,
                                     GLuint aGridDimension,
                                     FrameGraph & aGraph)
{
    // Requirement because on the shader side, we have to treat the SSBO 
    // as an array of uint (which are 4 bytes), and we store voxel per byte.
    assert((aGridDimension % 4) == 0);

    const float maxSide = *mSceneAabb.mDimension.getMaxMagnitudeElement();

    // We remap [[-halfSide, halfSide]^2, [-side, 0]] to [-1, 1]^3
    // Note that we also inverse the sign on Z axis, to change handedness 
    // (clip is left-handed)
    const math::Position<3, GLfloat> camOffset =
        -mSceneAabb.leftBottomZMin()
        - math::Vec<3, GLfloat>{maxSide / 2, maxSide / 2, maxSide / 2};
    const math::Vec<3, GLfloat> camScale = {2 / maxSide, 2 / maxSide, -2 / maxSide};

    std::size_t storeByteSize = VoxelsSsbo_glsl::ComputeByteSize(aGridDimension);
    mVoxelsByteSize = storeByteSize - offsetof(VoxelsSsbo_glsl, mVoxels);

    // Immutable: should be done only once, cannot resize
    //const GLbitfield flags = GL_MAP_READ_BIT;
    //glNamedBufferStorage(mVoxelStore, mStoreByteSize, nullptr, flags);
    // Mutable
    // TODO: chose the correct usage when we do not read from client anymore
    // (probably dynamic_copy)
    glNamedBufferData(mVoxelStore, storeByteSize, nullptr, mControl.mCpuReadVoxels ? GL_STREAM_READ : GL_DYNAMIC_COPY);

    // Note: is it usefull for a buffer that was just created?
    const std::uint8_t zero = 0;
    glClearNamedBufferData(mVoxelStore, GL_R8, GL_RED, GL_UNSIGNED_BYTE, &zero);

    // TODO: solve the warning regarding moving from video to host memory
    // (try mapping unsynchronized)
    // The grid dimension is the first data in the SSBO, we have to write it
    glNamedBufferSubData(mVoxelStore, offsetof(VoxelsSsbo_glsl, mGridDimension),
                         sizeof(GLuint), &aGridDimension);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, mVoxelStore);

    // 3D textures
    const GLenum gImageFormat = GL_RGBA8;
    const GLenum gAccessFormat = GL_R32UI;

    mAlbedo = prepare3dTexture(gImageFormat, aGridDimension, 1, "voxels_albedo");
    mNormals = prepare3dTexture(gImageFormat, aGridDimension, 1, "voxels_normal");

    glViewport(0, 0, aGridDimension, aGridDimension);

    auto & program = aGraph.mPrograms.mVoxelizationDominantAxisProgram;

    graphics::setUniform(program, "u_CameraOffset", camOffset);
    graphics::setUniform(program, "u_CameraScale", camScale);
    graphics::setUniform(program, "u_ConservativeDepthRange", mControl.mConservativeDepthRange);
    graphics::setUniform(program, "u_AverageSamples", mControl.mAverageSamples);
    graphics::setUniform(program, "u_AverageNormalByAxis", mControl.mAverageNormalByAxis);
    graphics::setUniform(program, "u_SeparateLightInjectionPass", mControl.mSeparateLightInjectionPass);

    // The image binding is not layered (GL_FALSE), and the texture does not have array layers:
    // layer parameter must be 0
    glBindImageTexture(gAlbedoImageUnit, mAlbedo,  0, GL_FALSE, 0, GL_READ_WRITE, gAccessFormat);
    glBindImageTexture(gNormalImageUnit, mNormals, 0, GL_FALSE, 0, GL_READ_WRITE, gAccessFormat);
    glBindImageTexture(gIrradianceImageUnit, mIrradiance, 0, GL_FALSE, 0, GL_READ_WRITE, gAccessFormat);

    graphics::setUniform(program, "u_AlbedoImage", gAlbedoImageUnit);
    graphics::setUniform(program, "u_NormalsImage", gNormalImageUnit);
    graphics::setUniform(program, "u_IrradianceImage", gIrradianceImageUnit);

    if (!mControl.mSeparateLightInjectionPass)
    {
        aGraph.setupShadowUniforms(program);
    }

    // Disable all operations on the Framebuffer
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    //glStencilMask(0);

    glDisable(GL_CULL_FACE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    auto rasterizationGuard = guardConservativeRasterization();
                
    drawPass(program, aScene, aGraph.mEngine);

    glBindImageTexture(gAlbedoImageUnit, 0, 0, GL_FALSE, 0, GL_READ_WRITE, gAccessFormat);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}


void Voxelizer::voxelizeDominantAxisView(const scenic::SceneTree & aScene, GLuint aGridDimension,
                                         FrameGraph & aGraph)
{
    const float maxSide = *mSceneAabb.mDimension.getMaxMagnitudeElement();

    // Not used by this draw pass, but will be used for debug drawing boxes
    graphics::loadSingle(mVoxelizationViewBuffer,
                         scenic::GpuViewProjectionBlock{
                             prepareVoxelizationCamera(mSceneAabb)},
                         graphics::BufferHint::StreamDraw);

    // We need to restore the previously bound viewprojection,
    // corresponding to the camera and assumed present by most of the code
    graphics::ScopedBind boundViewProjection{
        mVoxelizationViewBuffer,
        graphics::BindingIndex{ 0 }};

    const math::Position<3, GLfloat> camOffset =
        -mSceneAabb.leftBottomZMin()
        - math::Vec<3, GLfloat>{maxSide / 2, maxSide / 2, maxSide / 2};
    const math::Vec<3, GLfloat> camScale = {2 / maxSide, 2 / maxSide, -2 / maxSide};

    // Done by calling context
    //glViewport();

    auto & program = aGraph.mPrograms.mVoxelizationDominantAxisViewProgram;
    graphics::setUniform(program, "u_CameraOffset", camOffset);
    graphics::setUniform(program, "u_CameraScale", camScale);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);

    drawPass(program, aScene, aGraph.mEngine);
}


void Voxelizer::voxelize(const scenic::SceneTree & aScene, GLuint aGridDimension,
                         FrameGraph & aGraph)
{
    // Requirement because on the shader side, we have to treat the SSBO 
    // as an array of uint (which are 4 bytes), and we store voxel per byte.
    assert((aGridDimension % 4) == 0);

    const float maxSide = *mSceneAabb.mDimension.getMaxMagnitudeElement();

    // Not used by this draw pass, but will be used for debug drawing boxes
    graphics::loadSingle(mVoxelizationViewBuffer,
                         scenic::GpuViewProjectionBlock{
                             prepareVoxelizationCamera(mSceneAabb)},
                         graphics::BufferHint::StreamDraw);

    // We need to restore the previously bound viewprojection,
    // corresponding to the camera and assumed present by most of the code
    graphics::ScopedBind boundViewProjection{
        mVoxelizationViewBuffer,
        graphics::BindingIndex{ 0 }};

    std::size_t storeByteSize = VoxelsSsbo_glsl::ComputeByteSize(aGridDimension);
    mVoxelsByteSize = storeByteSize - offsetof(VoxelsSsbo_glsl, mVoxels);

    // Immutable: should be done only once, cannot resize
    //const GLbitfield flags = GL_MAP_READ_BIT;
    //glNamedBufferStorage(mVoxelStore, mStoreByteSize, nullptr, flags);
    // Mutable
    // TODO: chose the correct usage when we do not read from client anymore
    // (probably dynamic_copy)
    glNamedBufferData(mVoxelStore, storeByteSize, nullptr, mControl.mCpuReadVoxels ? GL_DYNAMIC_READ : GL_DYNAMIC_COPY);

    // Note: is it usefull for a buffer that was just created?
    const std::uint8_t zero = 0;
    glClearNamedBufferData(mVoxelStore, GL_R8, GL_RED, GL_UNSIGNED_BYTE, &zero);

    // TODO: solve the warning regarding moving from video to host memory
    // (try mapping unsynchronized)
    // The grid dimension is the first data in the SSBO, we have to write it
    glNamedBufferSubData(mVoxelStore, offsetof(VoxelsSsbo_glsl, mGridDimension),
                         sizeof(GLuint), &aGridDimension);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, mVoxelStore);

    glViewport(0, 0, aGridDimension, aGridDimension);

    auto & program = aGraph.mPrograms.mVoxelizationProgram;

    GLuint query;
    glGenQueries(1, &query);
    glBeginQuery(GL_FRAGMENT_SHADER_INVOCATIONS, query);

    // Disable all operations on the Framebuffer
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    //glStencilMask(0);

    glDisable(GL_CULL_FACE);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    auto rasterizationGuard = guardConservativeRasterization();

    drawPass(program, aScene, aGraph.mEngine);

    glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS);
    GLuint64 fragmentInvocations = 0;
    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &fragmentInvocations);
    std::cerr << "Voxelization FS invocations: " << fragmentInvocations << std::endl;

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void Voxelizer::voxelizeView(const scenic::SceneTree & aScene, GLuint aGridDimension,
                             FrameGraph & aGraph)
{
    // Requirement because on the shader side, we have to treat the SSBO 
    // as an array of uint (which are 4 bytes), and we store voxel per byte.
    assert((aGridDimension % 4) == 0);

    graphics::loadSingle(mVoxelizationViewBuffer,
                         scenic::GpuViewProjectionBlock{
                             prepareVoxelizationCamera(mSceneAabb)},
                         graphics::BufferHint::StreamDraw);

    // We need to restore the previously bound viewprojection,
    // corresponding to the camera and assumed present by most of the code
    graphics::ScopedBind boundViewProjection{
        mVoxelizationViewBuffer,
        graphics::BindingIndex{ 0 }};

    // Done by calling context
    //glViewport(0, 0, aGridDimension, aGridDimension);

    auto & program = aGraph.mPrograms.mVoxelizationViewProgram;

    GLuint query;
    glGenQueries(1, &query);
    glBeginQuery(GL_FRAGMENT_SHADER_INVOCATIONS, query);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);

    drawPass(program, aScene, aGraph.mEngine);

    glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS);
    GLuint64 fragmentInvocations = 0;
    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &fragmentInvocations);
    std::cerr << "Voxelization FS invocations: " << fragmentInvocations << std::endl;
}


void Voxelizer::bindAnisoTextures()
{
    const std::vector<GLuint> anisos{mIrradianceAnisoMipmaps.begin(),
                                     mIrradianceAnisoMipmaps.end()};
    glBindTextures(gAnisoIrradianceTextureUnit, 6, anisos.data());
}


void Voxelizer::prepareMipmap(GLuint aGridDimension, FrameGraph & aGraph)
{
    if (mControl.mComputeIrradianceMipmapping)
    {
        if (mControl.mAnisotropicIrradianceMipmapping)
        {
            // NOTE: we do the isotropic mipmapping in all cases,
            // because we use it for raytracing intersection test at all miplevels
            mipmapIrradiance(aGridDimension, aGraph);
            mipmapAnisotropicIrradiance(aGridDimension, aGraph);
        }
        else
        {
            mipmapIrradiance(aGridDimension, aGraph);
        }
    }
    else
    {
        glGenerateTextureMipmap(mIrradiance);
    }
}


void Voxelizer::mipmapIrradiance(GLuint aGridDimension, FrameGraph & aGraph)
{
    auto & program = aGraph.mPrograms.mFilterIrradianceProgram;
    glUseProgram(program);

    graphics::setUniform(program, "u_IrradianceSourceImage", gIrradianceImageUnit);
    graphics::setUniform(program, "u_IrradianceDestinationImage", gIrradianceMipmapImageUnit);

    // TODO: consolidate with the other calls
    GLsizei levels = 
        graphics::countCompleteMipmaps({(int)aGridDimension, (int)aGridDimension});

    math::Vec<3, GLuint> destinationDimension{aGridDimension, aGridDimension, aGridDimension};
    
    for(GLint sourceLevel = 0; sourceLevel + 1 != levels; ++sourceLevel)
    {
        glBindImageTexture(gIrradianceImageUnit, mIrradiance, sourceLevel,
                           GL_FALSE, 0, 
                           GL_READ_ONLY, gIrradianceFormat);
        glBindImageTexture(gIrradianceMipmapImageUnit, mIrradiance, sourceLevel + 1,
                           GL_FALSE, 0, 
                           GL_WRITE_ONLY, gIrradianceFormat);

        destinationDimension /= 2;
        // Note: There is something shady with GLSL imageSize, giving me very inconsistent results
        // (and a quick search shows an anormal volume of forum complaints)
        graphics::setUniform(program, "u_DestinationDimension", destinationDimension);

        math::Vec<3, GLfloat> numWorkgroups =
            destinationDimension.as<math::Vec, GLfloat>().cwDiv(gWorkgroupSize.as<math::Vec, GLfloat>());

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glDispatchCompute(std::ceil(numWorkgroups.x()),
                          std::ceil(numWorkgroups.y()),
                          std::ceil(numWorkgroups.z()));
    }
}


void Voxelizer::mipmapAnisotropicIrradiance(GLuint aGridDimension,
                                            FrameGraph & aGraph)
{

    // Destination dimension for level 0 of dedicated mipmap textures is half the initial grid dimension
    math::Vec<3, GLuint> destinationDimension{aGridDimension/2, aGridDimension/2, aGridDimension/2};
    GLsizei levels = 
        graphics::countCompleteMipmaps({(int)aGridDimension/2, (int)aGridDimension/2});

    //
    // Initial mipmap level: the level 0 of the anisotropic mipmap textures
    //
    const auto * program = &aGraph.mPrograms.mFilterIrradianceAnisoBaseProgram;
    glUseProgram(*program);

    // In this situation, the source is the same for all directions: level 0
    // of the irradiance 3D texture
    GLint sourceLevel = 0;
    glBindImageTexture(gIrradianceImageUnit, mIrradiance, sourceLevel,
                       GL_FALSE, 0, 
                       GL_READ_ONLY, gIrradianceFormat);
    graphics::setUniform(*program, "u_IrradianceSourceImage", gIrradianceImageUnit);

    // There is a distinct destination image for each direction (+X, -X, +Y, -Y, +Z, -Z)
    // Note: the destination level is the same as the source level 
    // (because we are writing to the first level of textures that are distinct from the source)
    for (int i = 0; i != 6; ++i)
    {
        glBindImageTexture(gIrradianceMipmapImageUnit + i, mIrradianceAnisoMipmaps[i], sourceLevel,
                           GL_FALSE, 0,
                           GL_WRITE_ONLY, gIrradianceFormat);

        // Useful for the second part where we will be reading 
        // from the lower level of the same aniso texture,
        glBindTextureUnit(gAnisoIrradianceTextureUnit + i, mIrradianceAnisoMipmaps[i]);
    }

    graphics::setUniform(*program, "u_DestinationDimension", destinationDimension);

    math::Vec<3, GLfloat> numWorkgroups =
        destinationDimension.as<math::Vec, GLfloat>().cwDiv(gWorkgroupSize.as<math::Vec, GLfloat>());

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glDispatchCompute(std::ceil(numWorkgroups.x()),
                      std::ceil(numWorkgroups.y()),
                      std::ceil(numWorkgroups.z()));

    //
    // Subsequent mipmap levels: the level 1..N of the anisotropic mipmap textures
    //
    program = &aGraph.mPrograms.mFilterIrradianceAnisoFromAnisoProgram;
    glUseProgram(*program);

    for(GLint sourceLevel = 0; sourceLevel + 1 != levels; ++sourceLevel)
    {
        // The destination images are in per-axis textures, at level source + 1
        for (int i = 0; i != 6; ++i)
        {
            glBindImageTexture(gIrradianceMipmapImageUnit + i, mIrradianceAnisoMipmaps[i],
                               sourceLevel + 1,
                               GL_FALSE, 0,
                               GL_WRITE_ONLY, gIrradianceFormat);
        }

        destinationDimension /= 2;
        graphics::setUniform(*program, "u_DestinationDimension", destinationDimension);
        graphics::setUniform(*program, "u_SourceLod", sourceLevel);

        math::Vec<3, GLfloat> numWorkgroups =
            destinationDimension.as<math::Vec, GLfloat>().cwDiv(gWorkgroupSize.as<math::Vec, GLfloat>());

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        glDispatchCompute(std::ceil(numWorkgroups.x()),
                          std::ceil(numWorkgroups.y()),
                          std::ceil(numWorkgroups.z()));
    }
}

void Voxelizer::prepareIrradianceTexture(GLuint aGridDimension)
{
    GLsizei totalLevels =
        graphics::countCompleteMipmaps({(int)aGridDimension, (int)aGridDimension});
    // Level 0 + isotropic mipmaps
    mIrradiance = prepare3dTexture(gIrradianceFormat,
                                   aGridDimension,
                                   totalLevels,
                                   "voxels_irradiance");

    if (mControl.mLinearFiltering)
    {
        glTextureParameteri(mIrradiance, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(mIrradiance, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    // The initial border color is vec4(0), which is okay for our use case
    glTextureParameteri(mIrradiance, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(mIrradiance, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTextureParameteri(mIrradiance, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);

    // Anisotropic mipmaps
    mIrradianceAnisoMipmaps.clear();
    mIrradianceAnisoMipmaps.reserve(6);
    for (unsigned int i = 0; i != 6; ++i)
    {
        mIrradianceAnisoMipmaps.push_back(
            prepare3dTexture(gIrradianceFormat,
                             aGridDimension / 2,
                             totalLevels - 1,
                             ("voxels_irradiance_aniso_" + std::to_string(i)).c_str()));

        auto & irradiance = mIrradianceAnisoMipmaps.back();
        if (mControl.mLinearFiltering)
        {
            glTextureParameteri(irradiance, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(irradiance, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        glTextureParameteri(irradiance, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(irradiance, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTextureParameteri(irradiance, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    }
}


void Voxelizer::injectIrradianceComputePass(GLuint aGridDimension, FrameGraph & aGraph)
{
    auto & program = aGraph.mPrograms.mInjectIrradianceProgram;
    glUseProgram(program);

    glBindTextureUnit(0, mAlbedo);
    glBindTextureUnit(1, mNormals);
    graphics::setUniform(program, "u_AlbedoTexture", 0);
    graphics::setUniform(program, "u_NormalsTexture", 1);
    glBindImageTexture(gIrradianceImageUnit, mIrradiance, 0,
                       GL_FALSE, 0,
                       GL_WRITE_ONLY, gIrradianceFormat);
    graphics::setUniform(program, "u_IrradianceImage", gIrradianceImageUnit);

    graphics::setUniform(program, "u_VoxelSize", mVoxelSize);
    graphics::setUniform(program, "u_AabbMin", mSceneAabb.leftBottomZMin());
    graphics::setUniform(program, "u_AverageNormalByAxis", mControl.mAverageNormalByAxis);

    const math::Vec<3, GLuint> totalInvocations{aGridDimension, aGridDimension, aGridDimension};
    math::Vec<3, GLuint> numWorkgroups = totalInvocations.cwDiv(gWorkgroupSize);

    glDispatchCompute(numWorkgroups.x(), numWorkgroups.y(), numWorkgroups.z());
}


void Voxelizer::fixupIrradianceAlphaComputePass(GLuint aGridDimension, FrameGraph & aGraph)
{
    auto & program = aGraph.mPrograms.mFixupIrradianceAlphaProgram;
    glUseProgram(program);

    glBindImageTexture(gIrradianceImageUnit, mIrradiance, 0,
                       GL_FALSE, 0,
                       GL_READ_WRITE, gIrradianceFormat);
    graphics::setUniform(program, "u_IrradianceImage", gIrradianceImageUnit);

    const math::Vec<3, GLuint> totalInvocations{aGridDimension, aGridDimension, aGridDimension};
    math::Vec<3, GLuint> numWorkgroups = totalInvocations.cwDiv(gWorkgroupSize);

    glDispatchCompute(numWorkgroups.x(), numWorkgroups.y(), numWorkgroups.z());
}


} // namespace ad