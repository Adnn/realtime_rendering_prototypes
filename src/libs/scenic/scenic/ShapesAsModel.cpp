#include "LoadScene.h"

#include "Shapes.h"
#include "VertexStreamUtilities.h"

#include <engine/SemanticValues.h>

#include <renderer/BufferLoad.h>


namespace ad::scenic {


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