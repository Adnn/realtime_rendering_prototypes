#pragma once


#include <engine/IntrospectProgram.h>


namespace ad {


struct Engine;

namespace scenic {
struct SceneTree;
} // namespace scenic

struct Voxelizer
{
    Voxelizer(Engine & aEngine);

    void voxelize(const scenic::SceneTree & aScene, unsigned int aGridSide);

    renderer::IntrospectProgram mProgram;
    graphics::Buffer<graphics::BufferType::ShaderStorage> mVoxelStore;
    GLsizeiptr mStoreByteSize;
};


} // namespace ad