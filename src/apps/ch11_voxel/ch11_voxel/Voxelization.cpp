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


void Voxelizer::voxelizeDominantAxis(const scenic::SceneTree & aScene, GLuint aGridDimension,
                                     const graphics::UniformBufferObject & aViewProjectionBuffer,
                                     const FrameGraph & aGraph)
{
    // Requirement because on the shader side, we have to treat the SSBO 
    // as an array of uint (which are 4 bytes), and we store voxel per byte.
    assert((aGridDimension % 4) == 0);

    const math::Box<float> sceneAabb = scenic::getAabb(aScene);
    // TODO: there is a duplication of the maxSide computation
    const float maxSide = *sceneAabb.mDimension.getMaxMagnitudeElement();

    // We remap [[-halfSide, halfSide]^2, [-side, 0]] to [-1, 1]^3
    // Note that we also inverse the sign on Z axis, to change handedness 
    // (clip is left-handed)
    const math::Position<3, GLfloat> camOffset =
        -sceneAabb.leftBottomZMin()
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
    const GLint gAlbedoImageUnit = 0;
    const GLenum gImageFormat = GL_RGBA8UI;
    const GLenum gAccessFormat = GL_R32UI;

    mAlbedo = {GL_TEXTURE_3D};
    {
        // For creation
        graphics::bind(mAlbedo);
        glObjectLabel(GL_TEXTURE, mAlbedo, -1, "voxels_albedo");

        // non-normalized integer texture should not use filtering
        glTextureParameteri(mAlbedo, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(mAlbedo, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        const GLsizei gLevels = 1;
        glTextureStorage3D(mAlbedo, gLevels, gImageFormat, aGridDimension, aGridDimension, aGridDimension);
    }

    // The image binding is not layered (GL_FALSE), and the texture does not have array layers:
    // layer parameter must be 0
    glBindImageTexture(gAlbedoImageUnit, mAlbedo, 0, GL_FALSE, 0, GL_READ_WRITE, gAccessFormat);

    glViewport(0, 0, aGridDimension, aGridDimension);

    const auto & program = aGraph.mPrograms.mVoxelizationDominantAxisProgram;
    graphics::setUniform(program, "u_CameraOffset", camOffset);
    graphics::setUniform(program, "u_CameraScale", camScale);
    graphics::setUniform(program, "u_ConservativeDepthRange", mControl.mConservativeDepthRange);

    graphics::setUniform(program, "u_AlbedoImage", gAlbedoImageUnit);

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
                                         const graphics::UniformBufferObject & aViewProjectionBuffer,
                                         const FrameGraph & aGraph)
{
    const math::Box<float> sceneAabb = scenic::getAabb(aScene);
    const float maxSide = *sceneAabb.mDimension.getMaxMagnitudeElement();

    // Not used by this draw pass, but will be used for debug drawing boxes
    graphics::loadSingle(aViewProjectionBuffer,
                         scenic::GpuViewProjectionBlock{
                             prepareVoxelizationCamera(sceneAabb)},
                         graphics::BufferHint::StreamDraw);

    const math::Position<3, GLfloat> camOffset =
        -sceneAabb.leftBottomZMin()
        - math::Vec<3, GLfloat>{maxSide / 2, maxSide / 2, maxSide / 2};
    const math::Vec<3, GLfloat> camScale = {2 / maxSide, 2 / maxSide, -2 / maxSide};

    // Done by calling context
    //glViewport();

    const auto & program = aGraph.mPrograms.mVoxelizationDominantAxisViewProgram;
    graphics::setUniform(program, "u_CameraOffset", camOffset);
    graphics::setUniform(program, "u_CameraScale", camScale);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);

    drawPass(program, aScene, aGraph.mEngine);
}


void Voxelizer::voxelize(const scenic::SceneTree & aScene, GLuint aGridDimension,
                         const graphics::UniformBufferObject & aViewProjectionBuffer,
                         const FrameGraph & aGraph)
{
    // Requirement because on the shader side, we have to treat the SSBO 
    // as an array of uint (which are 4 bytes), and we store voxel per byte.
    assert((aGridDimension % 4) == 0);

    const math::Box<float> sceneAabb = scenic::getAabb(aScene);
    // TODO: there is a duplication of the maxSide computation
    const float maxSide = *sceneAabb.mDimension.getMaxMagnitudeElement();

    graphics::loadSingle(aViewProjectionBuffer,
                         scenic::GpuViewProjectionBlock{
                             prepareVoxelizationCamera(sceneAabb)},
                         graphics::BufferHint::StreamDraw);

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

    const auto & program = aGraph.mPrograms.mVoxelizationProgram;
    graphics::setUniform(program, "u_AabbDepth", maxSide);

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
                             const graphics::UniformBufferObject & aViewProjectionBuffer,
                             const FrameGraph & aGraph)
{
    // Requirement because on the shader side, we have to treat the SSBO 
    // as an array of uint (which are 4 bytes), and we store voxel per byte.
    assert((aGridDimension % 4) == 0);

    const math::Box<float> sceneAabb = scenic::getAabb(aScene);
    const float maxSide = *sceneAabb.mDimension.getMaxMagnitudeElement();
    graphics::loadSingle(aViewProjectionBuffer,
                         scenic::GpuViewProjectionBlock{
                             prepareVoxelizationCamera(sceneAabb)},
                         graphics::BufferHint::StreamDraw);

    // Done by calling context
    //glViewport(0, 0, aGridDimension, aGridDimension);

    const auto & program = aGraph.mPrograms.mVoxelizationViewProgram;

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


void Voxelizer::prepareMipmap(GLuint aGridDimension)
{
    mOccupancy = {GL_TEXTURE_3D};
    // For creation
    graphics::bind(mOccupancy);

    if (mControl.mLinearFiltering)
    {
        glTextureParameteri(mOccupancy, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(mOccupancy, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    GLsizei levels = GLsizei(std::log2(aGridDimension)) + 1;
    glTextureStorage3D(mOccupancy, levels, GL_RGBA8, aGridDimension, aGridDimension, aGridDimension);

    std::uint8_t * buffer =
        (std::uint8_t *)glMapNamedBufferRange(mVoxelStore,
                                              offsetof(VoxelsSsbo_glsl, mVoxels),
                                              mVoxelsByteSize,
                                              GL_MAP_READ_BIT);
    std::vector<math::sdr::Rgba> textureData;
    textureData.reserve(mVoxelsByteSize);
    // Need to reorder: the buffer is stored Z-first
    //for (std::size_t idx = 0; idx != mVoxelsByteSize; ++idx)
    //{
    //    textureData.push_back({0, 0, 0, 
    //                          (std::uint8_t)((buffer[idx] == 1) ? 255 : 0)});
    //}

    unsigned int zStride = 1;
    unsigned int xStride = aGridDimension * zStride;
    unsigned int yStride = aGridDimension * xStride;

    // The buffer is stored Z-first, whereas the 3D textures are Z-last
    // We could either sample Z-first in the shader, or fix it here
    for (unsigned int z = 0; z != aGridDimension; ++z)
    {
        for (unsigned int y = 0; y != aGridDimension; ++y)
        {
            for (unsigned int x = 0; x != aGridDimension; ++x)
            {
                std::size_t idx = z * zStride + y * yStride + x * xStride;
                textureData.push_back({0, 0, 0, 
                                      (std::uint8_t)((buffer[idx] == 1) ? 255 : 0)});
            }
        }
    }

    glUnmapNamedBuffer(mVoxelStore);

    glTextureSubImage3D(mOccupancy, 0,
                        0, 0, 0, aGridDimension, aGridDimension, aGridDimension,
                        GL_RGBA, GL_UNSIGNED_BYTE, textureData.data());

    glGenerateTextureMipmap(mOccupancy);
}


} // namespace ad