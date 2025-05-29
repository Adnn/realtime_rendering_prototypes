#pragma once


#include <engine/IntrospectProgram.h>


namespace ad {


struct Engine;

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
    Voxelizer(Engine & aEngine);

    void voxelize(const scenic::SceneTree & aScene, GLuint aGridDimension);

    renderer::IntrospectProgram mProgram;
    graphics::Buffer<graphics::BufferType::ShaderStorage> mVoxelStore;
    GLsizeiptr mVoxelsByteSize;
    graphics::VertexArrayObject mDummyVao;
};


} // namespace ad