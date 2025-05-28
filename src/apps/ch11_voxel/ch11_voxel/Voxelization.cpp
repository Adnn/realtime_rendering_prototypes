#include "Voxelization.h"

#include "Engine.h"

#include <scenic/Model.h>

#include <renderer/BufferBase.h>
#include <renderer/Uniforms.h>


namespace ad {

namespace {

    const renderer::ReferencePath gVoxelizationProgram{"programs/ch11_Voxelization.prog"};

} // unnamed namespace


Voxelizer::Voxelizer(Engine & aEngine) :
    mProgram{aEngine.loadProgram(gVoxelizationProgram)}
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, mVoxelStore); // Generate the buffer
}


void Voxelizer::voxelize(const scenic::SceneTree & aScene, unsigned int aGridSide)
{
    // Requirement because on the shader side, we have to treat the SSBO 
    // as an array of uint (which are 4 bytes).
    assert((aGridSide % 4) == 0);

    math::Box<float> sceneAabb = scenic::getAabb(aScene);
    float maxSide = *sceneAabb.mDimension.getMaxMagnitudeElement();

    const unsigned int voxelCount = std::pow(aGridSide, 3);
    mStoreByteSize = voxelCount * sizeof(std::uint8_t);

    // Immutable: should be done only once, cannot resize
    //const GLbitfield flags = GL_MAP_READ_BIT;
    //glNamedBufferStorage(mVoxelStore, mStoreByteSize, nullptr, flags);
    // Mutable
    // TODO: chose the correct usage when we do not read from client anymore
    glNamedBufferData(mVoxelStore, mStoreByteSize, nullptr, GL_STREAM_READ);

    // Note: is it usefull for a buffer that was just created?
    const std::uint8_t zero = 0;
    glClearNamedBufferData(mVoxelStore, GL_R8, GL_RED, GL_UNSIGNED_BYTE, &zero);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, mVoxelStore);

    graphics::setUniform(mProgram, "u_GridSide", aGridSide);
    glUseProgram(mProgram);
    static const graphics::VertexArrayObject dummyVao;
    auto boundVao = graphics::ScopedBind{dummyVao};

    glViewport(0, 0, aGridSide, aGridSide);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


} // namespace ad