#pragma once


#include <engine/IntrospectProgram.h>

#include <renderer/UniformBuffer.h>


namespace ad {


struct Engine;
struct FrameGraph;

namespace scenic {
struct SceneTree;
} // namespace scenic

struct VoxelsSsbo_glsl
{
    static std::size_t ComputeByteSize(GLuint aGridDimension);

    GLuint mGridDimension;
    // TODO: Solve the problem of unbounded element in the glsl ssbo.
    // On one hand we would like the array size to be dynamic like a vector,
    // yet it means we cannot simply transfer the whole struct as a copy.
    // Moreover, the alignment for std::vector is 8,
    // whereas the voxel array alignment is 4 with std430.
    //alignas(4) std::vector<std::uint8_t> mVoxels;
    // WARNING: arbitrary incorrect size. We use this member for its offset.
    std::array<GLuint, 1> mVoxels;
};

struct Voxelizer
{
    struct VoxelizerControl
    {
        bool mUseDominantAxis = true;
        bool mConservativeRasterization = false;
    };

    Voxelizer();

    void voxelizeDominantAxis(const scenic::SceneTree & aScene, GLuint aGridDimension,
                              // TODO: take a FrameGraph (when the buffers are moved there)
                              const graphics::UniformBufferObject & aViewProjectionBuffer,
                              const FrameGraph & aGraph);

    void voxelizeDominantAxisView(const scenic::SceneTree & aScene, GLuint aGridDimension,
                                  const graphics::UniformBufferObject & aViewProjectionBuffer,
                                  const FrameGraph & aGraph);

    void voxelize(const scenic::SceneTree & aScene, GLuint aGridDimension,
                  // TODO: take a FrameGraph (when the buffers are moved there)
                  const graphics::UniformBufferObject & aViewProjectionBuffer,
                  const FrameGraph & aGraph);

    void voxelizeView(const scenic::SceneTree & aScene, GLuint aGridDimension,
                  // TODO: take a FrameGraph (when the buffers are moved there)
                  const graphics::UniformBufferObject & aViewProjectionBuffer,
                  const FrameGraph & aGraph);

    Guard guardConservativeRasterization();

    VoxelizerControl mControl;
    graphics::Buffer<graphics::BufferType::ShaderStorage> mVoxelStore;
    GLsizeiptr mVoxelsByteSize;
    graphics::VertexArrayObject mDummyVao;
};


} // namespace ad