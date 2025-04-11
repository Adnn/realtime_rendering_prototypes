#include "SetupDrawing.h"


#include "log/Logging.h"
#include "log/Logging-channels.h"

#include <engine/Semantic.h>
#include <engine/SemanticValues.h>

#include <renderer/ScopeGuards.h>

//#include <fmt/ostream.h>


namespace ad {

using namespace renderer;

namespace {


    /// @brief Attach vertex buffer data to a shader program parameter.
    ///
    /// Make several sanity checks regarding the data and attribute "compatibility"
    /// (dimensions, etc.)
    void attachAttribute(const renderer::IntrospectProgram::Attribute & aShaderAttribute,
                         graphics::ClientAttribute aClientAttribute, // taken by copy because we need to mutate locally
                         const scenic::BufferView & aVertexBufferView,
                         std::string_view aProgramName /* for log messages */)
    {
            // Consider secondary dimension mismatch (i.e., number of rows) a fatal error
            // Note: this might be too conservative, to be revised when a use case requires it.
            if (aClientAttribute.mDimension[1] != aShaderAttribute.dimension()[1])
            {
                ADLOG(critical)(
                    "{}: Program '{}' attribute '{}'({}) of dimension {} is to be attached to vertex data of dimension {}. Row counts are not allowed to differ.",
                    __func__,
                    aProgramName,
                    aShaderAttribute.mName,
                    to_string(aShaderAttribute.mSemantic),
                    fmt::streamed(aShaderAttribute.dimension()),
                    fmt::streamed(aClientAttribute.mDimension)
                );
                throw std::logic_error{"Mismatching row counts between shader attribute and buffer data."};
            }
            // We allow the primary dimension of the vertex attribute to mismatch the provided vertex data, but warn.
            else if (aClientAttribute.mDimension[0] != aShaderAttribute.dimension()[0])
            {
                ADLOG_LG(gPipelineDiag, warn)(
                    "{}: Program '{}' attribute '{}'({}) of dimension {} is to be attached to vertex data of dimension {}.",
                    __func__,
                    aProgramName,
                    aShaderAttribute.mName,
                    to_string(aShaderAttribute.mSemantic),
                    fmt::streamed(aShaderAttribute.dimension()),
                    fmt::streamed(aClientAttribute.mDimension)
                );
            }

            graphics::ScopedBind scopeVertexBuffer{*aVertexBufferView.mGLBuffer, graphics::BufferType::Array};
            // The client attribute, as provided by the VertexStream, contains the offset from the start of the BufferView
            // We need to add the offset of the BufferView from the start of the Buffer, before calling attachBoundVertexBuffer()
            aClientAttribute.mOffset += aVertexBufferView.mOffset;
            graphics::attachBoundVertexBuffer(
                {aShaderAttribute.toShaderParameter(), aClientAttribute},
                aVertexBufferView.mStride,
                aVertexBufferView.mInstanceDivisor);
    }


} // unnamed namespace


graphics::VertexArrayObject prepareVAO(const renderer::IntrospectProgram & aProgram,
                                       const scenic::MeshPart_Naive & aMesh)
{
    graphics::VertexArrayObject vertexArray;
    graphics::ScopedBind scopedVao{vertexArray};

    if (useElementIndices(aMesh))
    {
        const graphics::BufferAny & ibo = aMesh.mIndexBuffer;
        // Now that the VAO is bound, it can store the index buffer binding.
        // Note that this binding is permanent, no need to scope it.
        graphics::bind(ibo, graphics::BufferType::ElementArray);
    }

    for (const renderer::IntrospectProgram::Attribute & shaderAttribute : aProgram.mAttributes)
    {
        if(shaderAttribute.mSemantic == renderer::semantic::g_builtin)
        {
            continue;
        }
        if(auto found = aMesh.mSemanticToAttribute.find(shaderAttribute.mSemantic);
           found != aMesh.mSemanticToAttribute.end())
        {
            const scenic::MeshPart_Naive::Accessor_Naive & accessor = found->second;

            // create a dummy buffer view, to reuse existing API

            auto elementSize = accessor.mClientDataFormat.sizeBytes();
            // Otherwise we have to handle multi-location attributes, such as matrices
            assert(elementSize <= 16);
            const GLsizeiptr bufferSize = elementSize * aMesh.mVertexCount;
            scenic::BufferView dummyBufferView{
                .mGLBuffer = accessor.mBuffer,
                .mStride = elementSize,
                .mInstanceDivisor = 0, // All attributes from a Naive MeshPart are per vertex
                .mOffset = 0,
                .mSize = bufferSize, // The view has access to the provided range of elements
            };

            // Assertion below is not true anymore, since we merge everything into the VertexStream
            //assert(vertexBufferView.mInstanceDivisor == 0); // should always be 0 for the VertexStream as far as I imagine
            attachAttribute(shaderAttribute,
                            accessor.mClientDataFormat,
                            dummyBufferView,
                            aProgram.name());
        }
        else
        {
            ADLOG_LG(gPipelineDiag, warn)(
                "{}: Could not find an a vertex array buffer for semantic '{}' in program '{}'.", 
                __func__,
                to_string(shaderAttribute.mSemantic),
                aProgram.name());
        }
    }

    //ADLOG(debug)("Configured a new VAO.");

    return vertexArray;
}


#if 0
void setBufferBackedBlocks(const IntrospectProgram & aProgram,
                           const RepositoryUbo & aUniformBufferObjects)
{
    for (const IntrospectProgram::UniformBlock & shaderBlock : aProgram.mUniformBlocks)
    {
        if(auto found = aUniformBufferObjects.find(shaderBlock.mSemantic);
           found != aUniformBufferObjects.end())
        {
            bind(*found->second, shaderBlock.mBindingIndex);
        }
        else
        {
            // TODO since this function is currently called before each draw
            // this is much too verbose for a warning...
            SELOG_LG(gPipelineDiag, warn)(
                "{}: Could not find an a block uniform for block semantic '{}' in program '{}'.", 
                __func__,
                to_string(shaderBlock.mSemantic),
                aProgram.name());
        }
    }
}


void setTextures(const IntrospectProgram & aProgram,
                 const RepositoryTexture & aTextures)
{
    GLint textureImageUnit = 0;
    for (const IntrospectProgram::Resource & shaderUniform : aProgram.mUniforms)
    {
        if (graphics::isResourceSamplerType(shaderUniform.mType))
        {
            if(auto found = aTextures.find(shaderUniform.mSemantic);
               found != aTextures.end())
            {
                if (shaderUniform.mArraySize != 1)
                {
                    SELOG_LG(gPipelineDiag, error)(
                        "{}: '{}' program uniform '{}'({}) is a sampler array of size {}, setting uniform arrays is not supported.",
                        __func__,
                        aProgram.name(),
                        shaderUniform.mName,
                        to_string(shaderUniform.mSemantic),
                        shaderUniform.mArraySize
                    );
                }

                // TODO assertions regarding texture-sampler compatibility
                auto guardImageUnit = graphics::activateTextureUnitGuard(textureImageUnit);
                bind(*found->second); // should not be unbound when we exit the current scope.
                // TODO Ad 2023/11/30: Currently, we are assigning texture image units to sampler uniforms of the shader program
                // and binding the texture to the texture unit each time.
                // Assuming we will bind the texture to the TU each time, we can set the uniforms only once for the shader program lifetime 
                // (as long as the texture is bound to the same unit).
                graphics::setUniform(aProgram, shaderUniform.mLocation, textureImageUnit);
                ++textureImageUnit;
            }
            else
            {
                // TODO since this function is currently called before each draw
                // this is much to verbose for a warning...
                // In the case of samplers thought this is likely an error:
                // the sampler being available means it is probably used.
                SELOG_LG(gPipelineDiag, error)(
                    "{}: Could not find a texture for semantic '{}' in program '{}'.", 
                    __func__,
                    to_string(shaderUniform.mSemantic),
                    aProgram.name());
            }
        }
    }
}
#endif 0

} // namespace ad