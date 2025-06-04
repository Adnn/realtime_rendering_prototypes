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
    glNamedBufferData(mVoxelStore, storeByteSize, nullptr, GL_STREAM_READ);

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

    const auto & program = aGraph.mPrograms.mVoxelizationDominantAxisProgram;
    graphics::setUniform(program, "u_CameraOffset", camOffset);
    graphics::setUniform(program, "u_CameraScale", camScale);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    auto rasterizationGuard = guardConservativeRasterization();
                
    drawPass(program, aScene);
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
    glEnable(GL_CULL_FACE);

    drawPass(program, aScene);
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
    glNamedBufferData(mVoxelStore, storeByteSize, nullptr, GL_STREAM_READ);

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

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    auto rasterizationGuard = guardConservativeRasterization();

    drawPass(program, aScene);

    glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS);
    GLuint64 fragmentInvocations = 0;
    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &fragmentInvocations);
    std::cerr << "Voxelization FS invocations: " << fragmentInvocations << std::endl;
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
    glEnable(GL_CULL_FACE);

    drawPass(program, aScene);

    glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS);
    GLuint64 fragmentInvocations = 0;
    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &fragmentInvocations);
    std::cerr << "Voxelization FS invocations: " << fragmentInvocations << std::endl;
}


} // namespace ad