#include "Loader.h"

#include <engine/log/Logging.h>

#include <arte/dds/Dds.h>
#include <arte/dds/DdsEnums.h>
#include <arte/detail/Json.h>

#include <renderer/DdsGL.h>

#include <fmt/ranges.h>

#include <fstream> 

#include <cassert>


namespace {


    using namespace ad;


    struct CompressedBlockInfo
    {
        // TODO: rename it is overloaded to handle more than internal format...
        GLenum mInternalFormat; // Not sure this member should exist
        GLsizei mByteSize; 
        math::Size<2, GLsizei> mDimensions;
    };


    // TODO: move to a general lower level header
    CompressedBlockInfo getBlockInfo(GLenum aTextureTarget, GLenum aCompressedFormat)
    {
        // The block byte size is the reason sub-block image size
        // are not going under a minimum value: any image in this format is at least 1 block
        GLint blockByteSize = 0;
        glGetInternalformativ(aTextureTarget, aCompressedFormat, 
                              GL_TEXTURE_COMPRESSED_BLOCK_SIZE, 1, &blockByteSize);
        assert(blockByteSize % 8 == 0);
        // IMPORTANT: despite what OpenGL 4.6 core profile claims, it returns the size in bits, not bytes on 
        // my NVidia Windows driver.
        // However Linux is well behaved and returns the proper size, so the division is only for windows scrubs.
        if (blockByteSize == 8 * 16)
        {
            blockByteSize /= 8;
        }

        // As of this writting, all compressed block sizes we implement are 16 bytes.
        // Remove this assert when this is not true anymore.
        assert(blockByteSize == 16);

        // Sanity check: the block is 4x4
        GLint blockWidth = 0;
        glGetInternalformativ(aTextureTarget, aCompressedFormat, 
                              GL_TEXTURE_COMPRESSED_BLOCK_WIDTH, 1, &blockWidth);
        GLint blockHeight = 0;
        glGetInternalformativ(aTextureTarget, aCompressedFormat, 
                              GL_TEXTURE_COMPRESSED_BLOCK_HEIGHT, 1, &blockHeight);
        assert(blockWidth == blockHeight && blockHeight == 4);

        return {
            .mInternalFormat = aCompressedFormat,
            .mByteSize = blockByteSize,
            .mDimensions = {(GLsizei)blockWidth, (GLsizei)blockHeight},
        };
    }


    // TODO: move to a general lower level header
    GLsizei computeCompressedImageSize(math::Size<2, GLsizei> aImageDimensions,
                                       GLsizei aBlockByteSize,          
                                       math::Size<2, GLsizei> aBlockDimensions = {4, 4})
    {
        const math::Size<2, GLsizei> gCeilOffset = aBlockDimensions - math::Size<2, GLsizei>{1, 1};

        // Formula for image size in bytes:
        // ceil(<w>/4) * ceil(<h>/4) * blocksize.
        // taken and generalized from appendix of:
        // https://registry.khronos.org/OpenGL/extensions/ARB/ARB_texture_compression_bptc.txt
        // Ceil is implemented with the generalized +3 / 4 from:
        // https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-file-layout-for-textures
        return ((aImageDimensions + gCeilOffset).cwDiv(aBlockDimensions)).area() 
               * aBlockByteSize;
    }
    
    
    void loadDdsData(graphics::Texture & aTexture,
                               GLenum aLoadedTarget, // might be a specific cubemap face
                               math::Size<2, GLsizei> aMainImageDimensions,
                               const CompressedBlockInfo & aBlockInfo,
                               const arte::dds::Header aDdsHeader,
                               std::istream & aDataStream,
                               GLint aLayerIdx = -1 /* -1 implies 2D texture target*/)
    {
        bool isCompressed = (aBlockInfo.mDimensions != math::Size<2, GLsizei>{1, 1});

        // The image data should be 2D
        assert(aDdsHeader.h.dwDepth == 1);
        assert(!isCompressed ||
                (aDdsHeader.h_dxt10 
                && aDdsHeader.h_dxt10->resourceDimension == arte::DDS_DIMENSION_TEXTURE2D));

        const GLsizei imageByteSize = 
                computeCompressedImageSize(aMainImageDimensions, aBlockInfo.mByteSize, aBlockInfo.mDimensions);

        graphics::ScopedBind bound{aTexture};
        
        // Client buffer to retrieve the compressed image data
        std::unique_ptr<char []> imageData{new char[imageByteSize]};

        // TODO: Should we handle alignment?
        //Guard scopedAlignemnt = graphics::scopeUnpackAlignment(aInput.alignment);

        const GLint mipmapCount =
            ((aDdsHeader.h.dwFlags & arte::DDSD_MIPMAPCOUNT) == arte::DDSD_MIPMAPCOUNT) ?
                aDdsHeader.h.dwMipMapCount
                : 1;

        // We expect complete mipmaps to be provided, or none
        assert(
            mipmapCount == 1
            || mipmapCount == graphics::countCompleteMipmaps(aMainImageDimensions));

        math::Size<2, GLsizei> levelDimensions{aMainImageDimensions};
        GLsizei levelByteSize = imageByteSize;
        for(GLint level = 0; level != mipmapCount; ++level)
        {
            aDataStream.read(imageData.get(), levelByteSize);
            assert(aDataStream.good());

            if(aLayerIdx < 0) // Assumed to mean the target is a 2D texture type
            {
                if (isCompressed)
                {
                    glCompressedTexSubImage2D(
                        aLoadedTarget,
                        level,
                        0, 0, // x, y offsets
                        levelDimensions.width(),
                        levelDimensions.height(),
                        aBlockInfo.mInternalFormat,
                        levelByteSize,
                        imageData.get());
                }
                else
                {
                    glTexSubImage2D(
                        aLoadedTarget,
                        level,
                        0, 0, // x, y offsets
                        levelDimensions.width(),
                        levelDimensions.height(),
                        aBlockInfo.mInternalFormat,
                        GL_HALF_FLOAT,
                        imageData.get());
                }
            }
            else
            {
                // TODO: handle 3D equivalent
                assert(isCompressed);

                // For 3D texture, I am unaware of a use case to make it distinct atm.
                assert(aLoadedTarget == aTexture.mTarget);
                glCompressedTexSubImage3D(aLoadedTarget,
                                          level,
                                          0, 0, aLayerIdx, // x, y, z offsets
                                          levelDimensions.width(),
                                          levelDimensions.height(),
                                          aDdsHeader.h.dwDepth,
                                          aBlockInfo.mInternalFormat, 
                                          levelByteSize,
                                          imageData.get());
            }

            // Prepare next iteration
            levelDimensions = max((levelDimensions / 2), {1, 1});
            levelByteSize = computeCompressedImageSize(levelDimensions, aBlockInfo.mByteSize, aBlockInfo.mDimensions);
        }
    }

} // unnamed namespace


namespace ad::renderer {


graphics::Texture loadDds(const std::filesystem::path & aDds)
{
    ADLOG(debug)("Loading the DDS texture: {}", aDds.string());

    std::ifstream ddsStream{aDds, std::ios_base::in | std::ios_base::binary};
    if(!ddsStream.good())
    {
        throw std::runtime_error{"Unable to open DDS file: '" + aDds.string() + "'."};
    }

    arte::dds::Header header = arte::dds::readHeader(ddsStream);

    const math::Size<2, unsigned int> imageSize = arte::dds::getDimensions(header);

    // TODO: address this stuff in the call to getTextureTarget, that's hell
    //const GLenum target = graphics::getTextureTarget(header);
    const GLenum target = GL_TEXTURE_2D;

    GLenum internalFormat;
    CompressedBlockInfo blockInfo;
    if (header.h_dxt10)
    {
        internalFormat = graphics::getCompressedFormat(header);
        blockInfo = getBlockInfo(target, internalFormat);
    }
    else
    {
        blockInfo.mDimensions = {1, 1};
        // TODO: cleanly handle that in graphics lib, test the capacities etc
        // consolidate behind a single interface if possible for both paths
        if (header.h.ddspf.dwFourCC == 113)
        {
            internalFormat = GL_RGBA16F;
            // Uncompressed but we piggyback until refactor
            blockInfo.mByteSize = 8;
            blockInfo.mInternalFormat = GL_RGBA;
        }
    }

    graphics::Texture texture{target};
    graphics::ScopedBind boundTexture{texture};
    glTexStorage2D(texture.mTarget, 
                   header.h.dwMipMapCount,
                   internalFormat,
                   imageSize.width(), imageSize.height());
    { // scoping `isSuccess`
        GLint isSuccess;
        glGetTexParameteriv(texture.mTarget, GL_TEXTURE_IMMUTABLE_FORMAT, &isSuccess);
        if(!isSuccess)
        {
            ADLOG(error)("Cannot create immutable storage for texture to load '{}'.", aDds.string());
            throw std::runtime_error{"Error creating immutable storage for texture."};
        }
    }

    if(target == GL_TEXTURE_CUBE_MAP)
    {
        // For a cubemap, we need to load each face (complete with its mipmaps) in sequence
        for(unsigned int faceIdx = 0; faceIdx != 6; ++faceIdx)
        {
            loadDdsData(texture,
                        GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIdx,
                        imageSize.as<math::Size, GLsizei>(),
                        blockInfo,
                        header,
                        ddsStream);
        }
    }
    else
    {
        // Load the current texture target
        loadDdsData(texture,
                    texture.mTarget,
                    imageSize.as<math::Size, GLsizei>(),
                    blockInfo,
                    header,
                    ddsStream);
    }

    return texture;
}


graphics::Texture Loader::loadDds(const ReferencePath& aDdsFile)
{
    return renderer::loadDds(mFinder.pathFor(aDdsFile.mPath));
}


IntrospectProgram Loader::loadProgram(const ReferencePath & aProgFile,
                                      std::vector<graphics::MacroDefine> aDefines) const
{
    std::vector<std::pair<const GLenum, graphics::ShaderSource>> shaders;

    auto programPath = mFinder.pathFor(aProgFile.mPath);

    // TODO factorize and make more robust (e.g. test file existence)
    Json program;
    try 
    {
        program = Json::parse(std::ifstream{programPath});
    }
    catch (Json::parse_error &)
    {
        ADLOG(critical)("Rethrowing exception from attempt to parse Json from '{}'.", programPath.string());
        throw;
    }

    // TODO Copy the constant defines
    //aDefines.insert(aDefines.end(), gClientConstantDefines.begin(), gClientConstantDefines.end());

    // #resource_redesign There are 2 options for the shader path:
    // * relative to the prog file (with FileLookup)
    //graphics::FileLookup lookup{programPath};
    // * or in the "assets" prefixes of ResourceFinder. This is what we do here.

    // Handle the defines at the .prog level before we start preprocessing
    // the individual stages.
    for(std::string macro : program.value("defines", Json{}))
    {
        aDefines.push_back(std::move(macro));
    }

    for (auto [shaderStage, shaderFile] : program.items())
    {
        GLenum stageEnumerator;
        if(shaderStage == "defines")
        {
            // Handled before entering the loop
            continue;
        }
        else if(shaderStage == "vertex")
        {
            stageEnumerator = GL_VERTEX_SHADER;
        }
        else if(shaderStage == "fragment")
        {
            stageEnumerator = GL_FRAGMENT_SHADER;
        }
        else if(shaderStage == "geometry")
        {
            stageEnumerator = GL_GEOMETRY_SHADER;
        }
        else if(shaderStage == "tcs")
        {
            stageEnumerator = GL_TESS_CONTROL_SHADER;
        }
        else if(shaderStage == "tes")
        {
            stageEnumerator = GL_TESS_EVALUATION_SHADER;
        }
        else
        {
            ADLOG(critical)("Unable to map shader stage key '{}' to a program stage.", shaderStage);
            throw std::invalid_argument{"Unhandled shader stage key."};
        }
        
        shaders.emplace_back(
            stageEnumerator,
            graphics::ShaderSource::Preprocess(mFinder.pathFor(shaderFile), aDefines));
    }

    ADLOG(debug)("Compiling shader program from '{}', containing {} stages, {defines}.",
                 programPath.string(), shaders.size(),
                 fmt::arg("defines", aDefines.empty() ? 
                    "no defines" 
                    : fmt::format("with defines '{}'", fmt::join(aDefines, ", "))));

    return IntrospectProgram{shaders.begin(), shaders.end(), aProgFile.mPath.filename().string()};
}


graphics::ShaderSource Loader::loadShader(const ReferencePath & aShaderFile) const
{
    return graphics::ShaderSource::Preprocess(mFinder.pathFor(aShaderFile.mPath));
}


} // namespace ad::renderer
