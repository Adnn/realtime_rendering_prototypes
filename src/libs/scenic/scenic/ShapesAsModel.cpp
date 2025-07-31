#include "LoadScene.h"

#include "Shapes.h"
#include "VertexStreamUtilities.h"

#include <engine/SemanticValues.h>

#include <renderer/BufferLoad.h>


namespace ad::scenic {


Object makeCube()
{
    auto aabb = math::Box<float>::CenterOnOrigin({ 2.f, 2.f, 2.f });

    // Note: we use indexed rendering for homogeneity, even though
    // each index in the list is unique
    const GLuint vertexCount = (GLuint)std::size(cube::gIndices);
    MeshPart_Naive mesh{
        .mIndicesType = graphics::MappedGL_v<Index>,
        .mPrimitiveMode = GL_TRIANGLES,
        .mVertexCount = vertexCount,
        .mIndicesCount = vertexCount,
        .mAabb = aabb,
    };

    // Populate the 36 unique vertices with their values fetched from shape data
    std::vector<Position> positions;
    positions.reserve(vertexCount);
    std::vector<Normal> normals;
    normals.reserve(vertexCount);
    std::vector<Index> uniqueIndices;
    uniqueIndices.reserve(vertexCount);
    for (std::size_t indexIdx = 0; indexIdx != vertexCount; ++indexIdx)
    {
        Index idx = cube::gIndices[indexIdx];
        positions.push_back(cube::gVertices[idx]);
        // Indices are stored by face (6 per face), 
        // 1 normal is stored for each face, in the same order
        normals.push_back(cube::gNormals[indexIdx / 6]);
        uniqueIndices.push_back((Index)indexIdx);
    }

    mesh.mSemanticToAttribute.insert(
        makeLoadedAccessor_Naive(
            AttributeDescription{
                .mSemantic = renderer::semantic::gPosition,
                .mDimension = 3,
                .mComponentType = GL_FLOAT
            },
            std::span{positions},
            GL_STATIC_DRAW)
    );
    mesh.mSemanticToAttribute.insert(
        makeLoadedAccessor_Naive(
            AttributeDescription {
                .mSemantic = renderer::semantic::gNormal,
                .mDimension = 3,
                .mComponentType = GL_FLOAT
            },
            std::span{normals},
            GL_STATIC_DRAW)
    );

    graphics::load(mesh.mIndexBuffer, std::span{uniqueIndices}, graphics::BufferHint::StaticDraw);

    Object result{
        .mAabb = aabb,
    };
    result.mParts.push_back(std::move(mesh));
    return result;
}


Object makeSphere(unsigned int aSubdivisions)
{
    geodesic::Sphere sphere{aSubdivisions};

    auto aabb = math::Box<float>::CenterOnOrigin({ 2.f, 2.f, 2.f });

    MeshPart_Naive mesh{
        .mIndicesType = graphics::MappedGL_v<Index>,
        .mPrimitiveMode = GL_TRIANGLES,
        .mVertexCount = (GLuint)sphere.mVertices.size(),
        .mIndicesCount = (GLuint)sphere.mIndices.size(),
        .mAabb = aabb,
    };

    mesh.mSemanticToAttribute.insert(
        makeLoadedAccessor_Naive(
            AttributeDescription {
                .mSemantic = renderer::semantic::gPosition,
                .mDimension = 3,
                .mComponentType = GL_FLOAT
            },
            std::span{sphere.mVertices},
            GL_STATIC_DRAW)
    );
    graphics::load(mesh.mIndexBuffer, std::span{sphere.mIndices}, graphics::BufferHint::StaticDraw);

    Object result{
        .mAabb = aabb,
    };
    result.mParts.push_back(std::move(mesh));
    return result;
}


} // namespce ad::scenic