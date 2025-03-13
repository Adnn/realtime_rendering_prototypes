#pragma once

#include "Model.h"


namespace ad::scenic {


struct AttributeDescription
{
    renderer::Semantic mSemantic;
    // To replace wih graphics::ClientAttribute if we support interleaved buffers (i.e. with offset)
    graphics::AttributeDimension mDimension;
    GLenum mComponentType;   // data individual components' type.
};


/// @brief Return the byte size of an attribute.
inline GLuint getByteSize(AttributeDescription aAttribute)
{ 
    return aAttribute.mDimension.countComponents() 
        * graphics::getByteSize(aAttribute.mComponentType); 
}


graphics::BufferAny makeBuffer(GLsizei aElementSize,
                               GLsizeiptr aElementCount,
                               GLenum aHint);

#if 0
/// @brief Create a GL buffer of specified size (without loading data into it).
/// @return Buffer view to the buffer.
// TODO rename (this also wraps in a BufferView)
BufferView makeBufferGetView(GLsizei aElementSize,
                             GLsizeiptr aElementCount,
                             GLuint aInstanceDivisor,
                             GLenum aHint,
                             ModelStorage & aStorage);


Handle<ConfiguredProgram> storeConfiguredProgram(IntrospectProgram aProgram, ModelStorage & aStorage);


/*
 * Mid-level functions to manage VertexStreams
 */

/// @brief Add a new VertexStream to storage, optionally initializing it 
/// with the view and attributes from `aGenericStream`.
/// @param aGenericStream is intended to provide a stream containing per instance data.
Handle<VertexStream> primeVertexStream(ModelStorage & aStorage,
                                       const GenericStream & aGenericStream = {}); 



/// @brief Set the index buffer for `aStream`.
void setIndexBuffer(Handle<VertexStream> aStream, 
                    GLenum aIndexType,
                    Handle<const graphics::BufferAny> aIndexBuffer,
                    unsigned int aIndicesCount,
                    GLintptr aBufferOffset = 0);


/// @brief Add a single vertex attributes to `aVertexStream`.
void addVertexAttribute(Handle<VertexStream> aVertexStream, 
                        AttributeDescription aAttribute,
                        Handle<const graphics::BufferAny> aVertexBuffer,
                        unsigned int aVerticesCount,
                        GLintptr aBufferOffset = 0);


using InterleavedAttributeDescription = std::pair<AttributeDescription, size_t>;
/// @brief Add several vertex attributes to `aVertexStream`. Their data is interleaved in `aVertexBuffer`.
/// @param aElementStride The size of the client (Cpp) struct containing the interleaved data for a single vertex.
///        It is taken separately, since the struct might not match the sum size of attributes (padding, ...)
void addInterleavedAttributes(Handle<VertexStream> aVertexStream, 
                              GLsizei aElementStride,
                              std::span<const InterleavedAttributeDescription> aAttributesAndOffsets,
                              Handle<const graphics::BufferAny> aVertexBuffer,
                              unsigned int aVerticesCount,
                              GLuint aInstanceDivisor = 0,
                              GLintptr aBufferOffset = 0);


/*
 * High-level functions to manage VertexStreams
 */

/// @note High level method to provide a distinct buffer for each attribute stream at the moment (i.e. no interleaving).
Handle<VertexStream> makeVertexStream(unsigned int aVerticesCount,
                                      unsigned int aIndicesCount,
                                      GLenum aIndexType,
                                      std::span<const AttributeDescription> aBufferedStreams,
                                      ModelStorage & aStorage,
                                      const GenericStream & aStream);
#endif

} // namespace ad::scenic