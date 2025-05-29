#include "Voxelization.h"

#include "Engine.h"

#include <scenic/Model.h>

#include <renderer/BufferBase.h>
#include <renderer/Uniforms.h>


namespace ad {

namespace {

    const renderer::ReferencePath gVoxelizationProgram{"programs/ch11_Voxelization.prog"};

} // unnamed namespace


std::size_t VoxelsSsbo_glsl::ComputeByteSize(GLuint aGridDimension)
{
    const unsigned int voxelCount = std::pow(aGridDimension, 3);
    return offsetof(VoxelsSsbo_glsl, mVoxels) 
           + voxelCount * sizeof(std::uint8_t);
}


Voxelizer::Voxelizer(Engine & aEngine) :
    mProgram{aEngine.loadProgram(gVoxelizationProgram)}
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mVoxelStore); // Generate the buffer
}


void Voxelizer::voxelize(const scenic::SceneTree & aScene, GLuint aGridDimension)
{
    // Requirement because on the shader side, we have to treat the SSBO 
    // as an array of uint (which are 4 bytes), and we store voxel per byte.
    assert((aGridDimension % 4) == 0);

    std::size_t storeByteSize = VoxelsSsbo_glsl::ComputeByteSize(aGridDimension);
    mVoxelsByteSize = storeByteSize - offsetof(VoxelsSsbo_glsl, mVoxels);

    // Immutable: should be done only once, cannot resize
    //const GLbitfield flags = GL_MAP_READ_BIT;
    //glNamedBufferStorage(mVoxelStore, mStoreByteSize, nullptr, flags);
    // Mutable
    // TODO: chose the correct usage when we do not read from client anymore
    glNamedBufferData(mVoxelStore, storeByteSize, nullptr, GL_STREAM_READ);

    // Note: is it usefull for a buffer that was just created?
    const std::uint8_t zero = 0;
    glClearNamedBufferData(mVoxelStore, GL_R8, GL_RED, GL_UNSIGNED_BYTE, &zero);

    // The grid dimension is the first data in the SSBO, we have to write it
    glNamedBufferSubData(mVoxelStore, offsetof(VoxelsSsbo_glsl, mGridDimension),
                         sizeof(GLuint), &aGridDimension);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, mVoxelStore);

    glUseProgram(mProgram);
    auto boundVao = graphics::ScopedBind{mDummyVao};

    glViewport(0, 0, aGridDimension, aGridDimension);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


} // namespace ad