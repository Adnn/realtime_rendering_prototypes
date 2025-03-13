#include "VertexStreamUtilities.h"

#include <renderer/ScopeGuards.h>


namespace ad::scenic {


namespace {


    BufferView makeBufferView(Handle<const graphics::BufferAny> aBuffer,
                              GLsizei aElementSize,
                              GLsizeiptr aElementCount,
                              GLuint aInstanceDivisor,
                              GLintptr aOffsetIntoBuffer)
    {
        const GLsizeiptr bufferSize = aElementSize * aElementCount;

        return BufferView{
            .mGLBuffer = aBuffer,
            .mStride = aElementSize,
            .mInstanceDivisor = aInstanceDivisor,
            .mOffset = aOffsetIntoBuffer,
            .mSize = bufferSize, // The view has access to the provided range of elements
        };
    };


} // unnamed namespace


graphics::BufferAny makeBuffer(GLsizei aElementSize,
                               GLsizeiptr aElementCount,
                               GLenum aHint)
{
    graphics::BufferAny glBuffer; // glGenBuffers()
    // TODO: should we use glCreate*() instead of glGen*() in our wrappers?
    // Bind to create the buffer state
    glBindBuffer(GL_ARRAY_BUFFER, glBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    const GLsizeiptr bufferSize = aElementSize * aElementCount;
    glNamedBufferData(
        glBuffer,
        bufferSize,
        nullptr,
        aHint);

    return glBuffer;
}

#if 0
BufferView makeBufferGetView(GLsizei aElementSize,
                             GLsizeiptr aElementCount,
                             GLuint aInstanceDivisor,
                             GLenum aHint,
                             ModelStorage & aStorage)
{
    return makeBufferView(
        makeBuffer(aElementSize, aElementCount, aHint, aStorage),
        aElementSize,
        aElementCount,
        aInstanceDivisor,
        0/*offset*/);
};


Handle<VertexStream> primeVertexStream(ModelStorage & aStorage, const GenericStream & aGenericStream)
{

    aStorage.mVertexStreams.push_back({
        .mVertexBufferViews{aGenericStream.mVertexBufferViews},
        .mSemanticToAttribute{aGenericStream.mSemanticToAttribute},
    });

    return & aStorage.mVertexStreams.back();
}


void setIndexBuffer(Handle<VertexStream> aVertexStream, 
                    GLenum aIndexType,
                    Handle<const graphics::BufferAny> aIndexBuffer,
                    unsigned int aIndicesCount,
                    GLintptr aBufferOffset)
{
    aVertexStream->mIndexBufferView = makeBufferView(aIndexBuffer,
                                                     graphics::getByteSize(aIndexType),
                                                     aIndicesCount,
                                                     0,
                                                     aBufferOffset);
    aVertexStream->mIndicesType = aIndexType;
}


void addVertexAttribute(Handle<VertexStream> aVertexStream, 
                        AttributeDescription aAttribute,
                        Handle<const graphics::BufferAny> aVertexBuffer,
                        unsigned int aVerticesCount,
                        GLintptr aBufferOffset)
{
    BufferView attributeView = makeBufferView(aVertexBuffer,
                                              getByteSize(aAttribute),
                                              aVerticesCount,
                                              0, // This is a per vertex attribute, divisor is 0
                                              aBufferOffset);

    aVertexStream->mSemanticToAttribute.emplace(
        aAttribute.mSemantic,
        AttributeAccessor{
            .mBufferViewIndex = aVertexStream->mVertexBufferViews.size(), // view is added below
            .mClientDataFormat{
                .mDimension = aAttribute.mDimension,
                .mOffset = 0, // No interleaving is hardcoded at the moment
                .mComponentType = aAttribute.mComponentType,
            },
        }
    );

    aVertexStream->mVertexBufferViews.push_back(std::move(attributeView));
}


void addInterleavedAttributes(Handle<VertexStream> aVertexStream, 
                              GLsizei aElementStride,
                              std::span<const InterleavedAttributeDescription> aAttributesAndOffsets,
                              Handle<const graphics::BufferAny> aVertexBuffer,
                              unsigned int aVerticesCount,
                              GLuint aInstanceDivisor,
                              GLintptr aBufferOffset)
{
    BufferView attributeView = makeBufferView(aVertexBuffer,
                                              aElementStride,
                                              aVerticesCount,
                                              aInstanceDivisor,
                                              aBufferOffset);

    for(const auto & [attribute, offset] : aAttributesAndOffsets)
    {
        aVertexStream->mSemanticToAttribute.emplace(
            attribute.mSemantic,
            AttributeAccessor{
                .mBufferViewIndex = aVertexStream->mVertexBufferViews.size(), // view is added below
                .mClientDataFormat{
                    .mDimension = attribute.mDimension,
                    .mOffset = offset,
                    .mComponentType = attribute.mComponentType,
                },
            });
    }

    aVertexStream->mVertexBufferViews.push_back(std::move(attributeView));
}



//Handle<ConfiguredProgram> storeConfiguredProgram(IntrospectProgram aProgram, ModelStorage & aStorage)
//{
//    aStorage.mPrograms.push_back(ConfiguredProgram{
//        .mProgram = std::move(aProgram),
//        // TODO #perf Share Configs between programs that are compatibles (i.e., same input semantic and type at same attribute index)
//        // (For simplicity, we create one Config per program at the moment).
//        .mConfig = (aStorage.mProgramConfigs.emplace_back(), &aStorage.mProgramConfigs.back()),
//    });
//    return &aStorage.mPrograms.back();
//}


// TODO Ad 2024/11/15: Have a special "empty" vertex stream (e.g. vertex pulling)
// maybe static, so it reuse the VAO from ProgramConfig
Handle<VertexStream> makeVertexStream(unsigned int aVerticesCount,
                                      unsigned int aIndicesCount,
                                      GLenum aIndexType,
                                      std::span<const AttributeDescription> aBufferedStreams,
                                      ModelStorage & aStorage,
                                      const GenericStream & aStream)
{
    BufferView iboView;
    // TODO Ad 2023/10/11: Should we support smaller index types.
    if(aIndexType != GL_NONE)
    {
        iboView = makeBufferGetView(
            graphics::getByteSize(aIndexType),
            aIndicesCount,
            0,
            GL_STATIC_DRAW,
            aStorage);
    }

    // The consolidated vertex stream
    aStorage.mVertexStreams.push_back({
        .mVertexBufferViews{aStream.mVertexBufferViews},
        .mSemanticToAttribute{aStream.mSemanticToAttribute},
        .mIndexBufferView = iboView,
        .mIndicesType = aIndexType,
    });

    VertexStream & vertexStream = aStorage.mVertexStreams.back();

    for(const auto & attribute : aBufferedStreams)
    {
        const GLsizei attributeSize = 
            attribute.mDimension.countComponents() * graphics::getByteSize(attribute.mComponentType);

        BufferView attributeView = makeBufferGetView(
            attributeSize,
            aVerticesCount,
            0,
            GL_STATIC_DRAW,
            aStorage);

        vertexStream.mSemanticToAttribute.emplace(
            attribute.mSemantic,
            AttributeAccessor{
                .mBufferViewIndex = vertexStream.mVertexBufferViews.size(), // view is added next
                .mClientDataFormat{
                    .mDimension = attribute.mDimension,
                    .mOffset = 0, // No interleaving is hardcoded at the moment
                    .mComponentType = attribute.mComponentType,
                },
            }
        );

        vertexStream.mVertexBufferViews.push_back(attributeView);
    }

    return &vertexStream;
}
#endif

} // namespace ad::scenic