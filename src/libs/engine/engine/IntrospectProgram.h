#pragma once


#include "Semantic.h"

#include <renderer/BufferIndexedBinding.h>
#include <renderer/ShaderVertexAttribute.h>
#include <renderer/Shading.h>
#include <renderer/ShaderSource.h>
#include <renderer/VertexSpecification.h>

#include <iosfwd>
#include <string>
#include <vector>


namespace ad::renderer {


struct IntrospectProgram
{
    using TypedShaderSource = std::pair<const GLenum/*stage*/, graphics::ShaderSource>;

    IntrospectProgram(
        graphics::Program aProgram,
        std::string aName,
        std::vector<TypedShaderSource> aSources);

    // TODO Ad 2023/11/09: Redesign the construction API
    // Requires including ShaderSource.h, which might worsen compilation times.
    template <std::forward_iterator T_iterator>
    IntrospectProgram(T_iterator aFirstShader, const T_iterator aLastShader, std::string aName) :
        IntrospectProgram{graphics::makeLinkedProgram(aFirstShader, aLastShader),
                          std::move(aName),
                          {aFirstShader, aLastShader}}
    {}

    IntrospectProgram(std::initializer_list<TypedShaderSource> aShaders, std::string aName) :
        IntrospectProgram{aShaders.begin(), aShaders.end(), std::move(aName)}
    {}

    /*implicit*/ operator graphics::Program &()
    { return mProgram; }

    /*implicit*/ operator const graphics::Program &() const
    { return mProgram; }

    /*implicit*/ operator GLuint() const
    { return mProgram; }

    std::string_view name() const
    { return mName; }

    struct Resource
    {
        GLint mLocation; // keep it signed, so it can hold -1 for uniforms from uniform blocks.
        GLenum mType;
        Semantic mSemantic;
        GLuint mArraySize;
        std::string mName; // To ease debugger inspection, we only need the semantic

        graphics::AttributeDimension dimension() const
        { return graphics::getResourceDimension(mType); }

        GLenum componentType() const
        { return graphics::getResourceComponentType(mType); }
    };

    struct Attribute : public Resource
    {
        graphics::ShaderParameter toShaderParameter() const;
        bool mIsNormalized;
    };

    struct UniformBlock
    {
        GLuint mBlockIndex;
        graphics::BindingIndex mBindingIndex;
        BlockSemantic mSemantic;
        std::string mName;
        // TODO do we want to use another type that does not have a location data member?
        //std::vector<Resource> mUniforms; // Uniform blocks contain uniform variables
    };

    graphics::Program mProgram;
    std::vector<Attribute> mAttributes;
    std::vector<Resource> mUniforms;
    std::vector<UniformBlock> mUniformBlocks;
    std::string mName;
    std::vector<TypedShaderSource> mSources; // Notably usefull for productivity tooling / hot-reloading.
};


std::ostream & operator<<(std::ostream & aOut, const IntrospectProgram & aProgram);
std::ostream & operator<<(std::ostream & aOut, const IntrospectProgram::Resource & aResource);
std::ostream & operator<<(std::ostream & aOut, const IntrospectProgram::UniformBlock & aBlock);


} // namespace ad::renderer