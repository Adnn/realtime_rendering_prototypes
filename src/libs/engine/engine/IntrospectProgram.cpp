#include "IntrospectProgram.h"

#include "Semantic.h"
#include "SemanticValues.h"

#include "log/Logging.h"

#include <handy/StringUtilities.h>

#include <array>
#include <iterator>
#include <memory>
#include <ostream>
#include <set>
#include <string>

#include <cassert>


namespace ad::renderer {


namespace {


    template <GLenum ... VN_properties>
    class InterfaceIterator
    {
        static constexpr std::size_t gPropertiesCount = sizeof...(VN_properties);

    public:
        using difference_type = GLint;

        struct value_type
        {
            GLint operator[](std::size_t aPropertyIndex) const
            {
                assert(aPropertyIndex < gPropertiesCount);
                return mProperties[aPropertyIndex];
            }

            std::string mName;
            GLuint mIndex;
            std::array<GLint, sizeof...(VN_properties) + 1 /*name length*/> mProperties;
        };

        using pointer = const value_type *;
        using reference = const value_type &;

        InterfaceIterator(const graphics::Program & aProgram, GLenum aProgramInterface):
            mProgram{&aProgram},
            mProgramInterface{aProgramInterface}
        {
            // P2325R2 defect report remove the requirement to be default_initializable.
            // Note: I am not sure this is implemented above gnuc 10, might be updated.
            #if (defined(_MSC_VER) || __clang_major__ > 13 || __GNUC__ > 10)//DR_P2325R3
            // TODO maybe could be bidirectional,
            // depending on the requireded validity for returned reference.
            // (the reference changes target on each call to operator*() after ++/--,
            //  and the reference becomes dangling when the iterator is destroyed.)
            static_assert(std::input_iterator<InterfaceIterator>);
            #endif
        }

        static InterfaceIterator makeEnd(const graphics::Program & aProgram, GLenum aProgramInterface)
        {
            InterfaceIterator end(aProgram, aProgramInterface);
            glGetProgramInterfaceiv(*end.mProgram, end.mProgramInterface, 
                                    GL_ACTIVE_RESOURCES, &end.mCurrentId);
            return end;
        }

        // Prefix increment
        InterfaceIterator & operator++()
        {
            ++mCurrentId;
            return *this;
        }

        // Postfix increment
        InterfaceIterator operator++(int)
        {
            InterfaceIterator old = *this;
            operator++();
            return old;
        }

        bool operator!=(const InterfaceIterator & aRhs) const
        {
            assert(mProgram == aRhs.mProgram);
            assert(mProgramInterface == aRhs.mProgramInterface);
            return mCurrentId != aRhs.mCurrentId;
        }

        reference operator*() const
        {
            const GLenum properties[] = {
                VN_properties...,
                GL_NAME_LENGTH, // put last, because we respect the order of VN_properties
            };
            GLint length;

            glGetProgramResourceiv(*mProgram, mProgramInterface, mCurrentId,
                                   (GLsizei)std::size(properties), properties,
                                   (GLsizei)std::size(mValue.mProperties), &length, mValue.mProperties.data());
            assert(static_cast<std::size_t>(length) == std::size(properties)); // Sanity check

            // The returned length makes provision for a terminating null character.
            // this character will be written to the buffer by glGetProgramResourceName()
            GLint & nameLength = mValue.mProperties[gPropertiesCount];
            auto nameBuffer = std::make_unique<char[]>(nameLength);
            // This will write the length **without** terminating null character.
            glGetProgramResourceName(*mProgram, mProgramInterface, mCurrentId,
                                     nameLength, &nameLength, nameBuffer.get());
            mValue.mName = std::string(nameBuffer.get(), nameLength);
            mValue.mIndex = (GLuint)mCurrentId;

            // TODO This raises the question of how long should the reference be valid by standard?
            return mValue;
        }

        pointer operator->()
        {
            // TODO this is a pessimisation, as it might dereference for each member access.
            return &(this->operator*());
        }

    private:
        const graphics::Program * mProgram;
        GLenum mProgramInterface;
        GLint mCurrentId{0};
        // It seems we must store the value, because operator*() has to return true references.
        // see: https://stackoverflow.com/a/52856349/1027706
        mutable value_type mValue; // needs to be written to by operator*() const.
    };


    Semantic to_semantic(std::string_view aResourceName)
    {
        static const std::string delimiter = "_";
        std::string_view prefix, body, suffix;
        std::tie(prefix, body) = lsplit(aResourceName, delimiter);
        std::tie(body, suffix) = lsplit(body, delimiter);

        // TODO: do we want to store the builtin semantics, as currently?
        // Should we discard them when they are encounters?
        if(prefix == "gl")
        {
            return semantic::g_builtin;
        }
        else
        {
            return std::string{body};
        }
    }


    BlockSemantic to_blockSemantic(std::string_view aBlockName)
    {
        if (aBlockName.ends_with(']'))
        {
            //TODO Ad 2023/07/06: support array of blocks.
            throw std::logic_error{
                "Unable to map shader block name '" + std::string{aBlockName} + "' to a semantic, arrays of blocks are not supported."};
        }
        else if(aBlockName.ends_with("Block"))
        {
            aBlockName.remove_suffix(5);
            return std::string{aBlockName};
        }
        else
        {
            throw std::logic_error{
                "Unable to map shader block name '" + std::string{aBlockName} + "' to a semantic."};
        }
    } 

    bool is_normalized(std::string_view aResourceName)
    {
        //TODO Ad 2023/07/06: This duplicates the logic from to_semantic, and is called in sequence.
        static const std::string delimiter = "_";
        std::string_view prefix, body, suffix;
        std::tie(prefix, body) = lsplit(aResourceName, delimiter);
        std::tie(body, suffix) = lsplit(body, delimiter);
        return suffix.starts_with("normalized");
    }

} // unnamed namespace


std::ostream & operator<<(std::ostream & aOut, const IntrospectProgram & aProgram)
{
    auto lister = [&aOut](std::string_view aCategory, const auto & aCollection)
    {
        aOut << "\n* " << aCollection.size() << " " << aCategory 
             << (aCollection.size() > 1 ? "s" : "") << ":";
        for (const auto & resource : aCollection)
        {
            aOut << "\n\t*" << resource;
        }
    };

    aOut << "Program '" << aProgram.mName << "' with:";
    lister("attribute", aProgram.mAttributes);
    lister("uniform", aProgram.mUniforms);
    lister("uniform block", aProgram.mUniformBlocks);
    return aOut;
}


std::ostream & operator<<(std::ostream & aOut, const IntrospectProgram::Resource & aResource)
{
    aOut << aResource.mName 
         << " (" << aResource.mSemantic << ")"
         ;
    
    if (aResource.mLocation != -1)
    {
        aOut << " at location " << aResource.mLocation;
    }

    return aOut << " has type " << graphics::to_string(graphics::getResourceComponentType(aResource.mType))
                << " and dimension " << graphics::getResourceDimension(aResource.mType)
                << "."
                ;
}


std::ostream & operator<<(std::ostream & aOut, const IntrospectProgram::UniformBlock & aBlock)
{
    aOut << aBlock.mName 
         // TODO add block semantic output
         //<< " (" << to_string(aResource.mSemantic) << ")"
         << " at binding index " << aBlock.mBindingIndex << "."
         ;
    return aOut;
}


IntrospectProgram::IntrospectProgram(graphics::Program aProgram,
                                     std::string aName,
                                     std::vector<TypedShaderSource> aSources) :
    mProgram{std::move(aProgram)},
    mName{std::move(aName)},
    mSources{std::move(aSources)}
{
    glObjectLabel(GL_PROGRAM, mProgram, -1, mName.c_str());

    auto makeResource = [](const auto &aAttribute) -> Resource
    {
        return Resource{
            .mLocation = aAttribute[0],
            .mType = (GLenum)aAttribute[1],
            .mSemantic = to_semantic(aAttribute.mName),
            .mArraySize = (GLuint)aAttribute[2],
            .mName = aAttribute.mName,
        };
    };

    auto makeAttribute = [&makeResource](const auto &aAttribute) -> Attribute
    {
        Attribute result{makeResource(aAttribute)};
        result.mIsNormalized = is_normalized(aAttribute.mName);
        return result;
    };

    // Attributes
    {
        using Iterator = InterfaceIterator<GL_LOCATION, GL_TYPE, GL_ARRAY_SIZE>;
        for(auto it = Iterator(mProgram, GL_PROGRAM_INPUT);
            it != Iterator::makeEnd(mProgram, GL_PROGRAM_INPUT);
            ++it)
        {
            mAttributes.push_back(makeAttribute(*it));
        }
    }

    // Uniform blocks
    {
        using Iterator = InterfaceIterator<GL_BUFFER_BINDING>;
        for (auto it = Iterator(mProgram, GL_UNIFORM_BLOCK);
            it != Iterator::makeEnd(mProgram, GL_UNIFORM_BLOCK);
            ++it)
        {
            mUniformBlocks.push_back(UniformBlock{
                .mBlockIndex = it->mIndex,
                .mBindingIndex = graphics::BindingIndex{(GLuint)(*it)[0]},
                .mSemantic = to_blockSemantic(it->mName),
                .mName = it->mName,
            });
        }

        // Note: When uniform blocks are not explicitly assigned a binding index
        // there tends to be a lot of collisions.
        // TODO Understand why, unlike for generic vertex attributes, automatic indices
        // are not working for uniform blocks.
        // TODO Ad 2024/02/22: Anyway, it seems what we might do is algorithmically
        // assign distinct indices to the blocks, instead of hardcoding values.
        // (and by being clever, we could minimize the need to change the buffer binding indices client side)
        
        [[maybe_unused]] // Otherwise clang complains that this variable is unused in release builds
        auto checkDuplicateIndex = [](const std::vector<UniformBlock> & aBlocks) -> bool
        {
            std::set<graphics::BindingIndex> boundIndices;
            for(const auto & block : aBlocks)
            {
                if (boundIndices.contains(block.mBindingIndex))
                {
                    return false;
                }
                boundIndices.insert(block.mBindingIndex);
            }
            return true;
        };
        assert(checkDuplicateIndex(mUniformBlocks));
    }

    // All uniforms ("normal" non-block uniforms, as well as within uniform blocks)
    {
        using Iterator = InterfaceIterator<GL_LOCATION, GL_TYPE, GL_ARRAY_SIZE, GL_BLOCK_INDEX>;
        for (auto it = Iterator(mProgram, GL_UNIFORM);
            it != Iterator::makeEnd(mProgram, GL_UNIFORM);
            ++it)
        {
            GLint blockIndex = (*it)[3];
            // **Not** part of an interface block
            if (blockIndex == -1) // BLOCK_INDEX == -1 means not part of an interface blocks
            {
                assert((*it)[0] != -1);
                mUniforms.push_back(makeResource(*it));
            }
            // Part of an inteface block
            else
            {
                // Note: Instead of re-iterating all uniforms of a block by uniform id
                //       (obtained via GL_ACTIVE_VARIABLES), match-up on block index.
                assert((*it)[0] == -1);
                auto found = std::find_if(mUniformBlocks.begin(), mUniformBlocks.end(),
                                          [blockIndex](const auto & block)
                                          {
                                            return block.mBlockIndex == (GLuint)blockIndex;
                                          });
                if (found == mUniformBlocks.end())
                {
                    // This is a thing that should not be
                    throw std::logic_error("Uniform found with non-existent block index.");
                }
                else
                {
                    // Note: Do not store the uniforms found inside an uniform block
                    // as uniform resources in the IntrospectProgram.
                    // I do not know if it could have a use case?
                    //found->mUniforms.push_back(makeResource(*it));
                }
            }
        }
    }
}


graphics::ShaderParameter IntrospectProgram::Attribute::toShaderParameter() const
{
    graphics::ShaderParameter result{
        (GLuint)mLocation,
        graphics::getResourceShaderAccess(mType)
    };
    result.mNormalize = mIsNormalized;
    return result;
}


} // namespace ad::renderer