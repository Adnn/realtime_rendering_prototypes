#pragma once


#include <engine/IntrospectProgram.h>

#include <scenic/Model.h>


namespace ad {


struct VertexStream;
struct InstanceStream;
struct IntrospectProgram;
struct GenericStream;


graphics::VertexArrayObject prepareVAO(const renderer::IntrospectProgram & aProgram,
                                       const scenic::MeshPart_Naive & aMesh);


#if 0
void setBufferBackedBlocks(const IntrospectProgram & aProgram,
                           const RepositoryUbo & aUniformBufferObjects);


void setTextures(const IntrospectProgram & aProgram,
                 const RepositoryTexture & aTextures);
#endif


} // namespace ad