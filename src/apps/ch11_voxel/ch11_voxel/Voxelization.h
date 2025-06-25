#pragma once


#include <engine/IntrospectProgram.h>

#include <math/Box.h>

#include <renderer/Texture.h>
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
        bool mConservativeRasterization = true;
        bool mConservativeDepthRange = false;
        bool mAverageSamples = true;
        bool mAverageNormalByAxis = true;
        // Not intended for GUI, but internal value updated depending on the usage context
        bool mCpuReadVoxels = false;
        bool mLinearFiltering = true;
    };

    Voxelizer();

    void voxelizeDominantAxis(const scenic::SceneTree & aScene,
                              GLuint aGridDimension,
                              const FrameGraph & aGraph);

    void voxelizeDominantAxisView(const scenic::SceneTree & aScene, GLuint aGridDimension,
                                  const FrameGraph & aGraph);

    void voxelize(const scenic::SceneTree & aScene, GLuint aGridDimension,
                  const FrameGraph & aGraph);

    void voxelizeView(const scenic::SceneTree & aScene, GLuint aGridDimension,
                      const FrameGraph & aGraph);

    void prepareMipmap(GLuint aGridDimension);

    void injectIrradiance(GLuint aGridDimension, const FrameGraph & aGraph);

    Guard guardConservativeRasterization();

    void recordSceneAabb(const scenic::SceneTree & aScene, GLuint aGridDimension);

    VoxelizerControl mControl;
    graphics::Buffer<graphics::BufferType::ShaderStorage> mVoxelStore;
    GLsizeiptr mVoxelsByteSize;
    math::Box<float> mSceneAabb; // Note: we could only keep min corner
    float mVoxelSize{0.f}; // World units
    graphics::Texture mOccupancy{GL_TEXTURE_3D};
    graphics::Texture mAlbedo{GL_TEXTURE_3D};
    graphics::Texture mNormals{GL_TEXTURE_3D};
    graphics::Texture mIrradiance{GL_TEXTURE_3D};
    graphics::VertexArrayObject mDummyVao;
    graphics::UniformBufferObject mVoxelizationViewBuffer;
};


} // namespace ad