#include "Loader.h"

#include "ShaderInclusionLookup.h"

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
    using namespace ad::graphics;


    // TODO: move that to a low level utils lib (handy)
    constexpr std::uint32_t makeFourCC(char a, char b, char c, char d)
    {
        return (std::uint32_t)a 
            | ((std::uint32_t)b << 8)
            | ((std::uint32_t)c << 16)
            | ((std::uint32_t)d << 24)
            ;
    }


    // Allows to test a combination of bits against a flag.
    template <class T, class U> 
    requires std::is_convertible_v<U, T>
    bool isFlagged(const T aFlags, const U aTestedBits)
    {
        return (aFlags & aTestedBits) == aTestedBits;
    }


    // TODO: move that to te gl low-level library (replace existing function)
    using namespace arte;
    GLenum getTextureFormat(const dds::Header & aHeader)
    {
        if(aHeader.h_dxt10)
        {
            const DDS_HEADER_DXT10 & dxt10 = *aHeader.h_dxt10;
            switch(dxt10.dxgiFormat)
            {
                default:
                    // TODO Ad 2024/07/24: Extend to support a reasonable set of formats.
                    //ADLOG(error)("DXGI format {} is not supported at the moment", dxt10.dxgiFormat)
                    throw std::domain_error("The texture format in this DDS is not supported at the moment.");
                case DXGI_FORMAT_BC5_UNORM:
                    return GL_COMPRESSED_RG_RGTC2;
                case DXGI_FORMAT_BC5_SNORM:
                    return GL_COMPRESSED_SIGNED_RG_RGTC2;
                case DXGI_FORMAT_BC6H_UF16:
                    return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
                case DXGI_FORMAT_BC6H_SF16:
                    return GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
                case DXGI_FORMAT_BC7_UNORM:
                    return GL_COMPRESSED_RGBA_BPTC_UNORM;
                case DXGI_FORMAT_BC7_UNORM_SRGB:
                    return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
            }
        }
        else if(isFlagged(aHeader.h.dwFlags, DDPF_FOURCC))
        {
            switch (aHeader.h.ddspf.dwFourCC)
            {
                default: 
                    // TODO Ad 2024/07/24: Extend to support a reasonable set of formats.
                    //ADLOG(error)("Four CC value {} is not supported at the moment", aHeader.h.ddspf.dwFourCC)
                    throw std::domain_error("The texture format in this DDS is not supported at the moment.");

                // see: https://github.com/microsoft/DirectXTex/blob/51f33c3471e4da2a2d235c8e4a745700644504a8/DDSTextureLoader/DDSTextureLoader12.cpp#L990-L1012
                // Important: Even though the D3D name is given with most-significant bit first,
                // the data is stored least-significant bit first (i.e. little endian).
                // see: https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dformat#remarks
                // So the color channels order match (it is RGBA), and if the system is little-endian
                // the byte order also matches inside each channel.

                // TODO: complete other formats if the need show up 
                // (I do not want to write to many untested cases)
                //case 36: // D3DFMT_A16B16G16R16
                //    return DXGI_FORMAT_R16G16B16A16_UNORM;
                //case 110: // D3DFMT_Q16W16V16U16
                //    return DXGI_FORMAT_R16G16B16A16_SNORM;
                case 111: // D3DFMT_R16F
                    return GL_R16F;
                case 112: // D3DFMT_G16R16F
                    return GL_RG16F;
                case 113: // D3DFMT_A16B16G16R16F
                    return GL_RGBA16F;
                //case 114: // D3DFMT_R32F
                //    return DXGI_FORMAT_R32_FLOAT;
                //case 115: // D3DFMT_G32R32F
                //    return DXGI_FORMAT_R32G32_FLOAT;
                //case 116: // D3DFMT_A32B32G32R32F
                //    return DXGI_FORMAT_R32G32B32A32_FLOAT;

                // TODO require extension EXT_texture_compression_s3tc
                //case makeFourCC('D', 'X', 'T', '1'):
                //    return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
            }
        }
        else
        {

        }
        throw std::invalid_argument("This DDS does not contain an extended DXT10 header.");
    }


    struct ImageBlockInfo
    {
        bool mIsCompressed;
        GLenum mInternalFormat; // The internal format (i.e. sized format to use with glTextStorage)
        GLenum mTexImageFormat; // The format to use with glCompressedTeximage (size) / glTexImage (non-sized)
        GLenum mTexImageType = GL_NONE; // The data type of the pixel data in the client memory (type argument to glTexSubImage())
        GLsizei mByteSize; // Block byte size
        math::Size<2, GLsizei> mDimensions; // Block dimensions
    };


    // TODO: move to a general lower level header
    ImageBlockInfo getCompressedBlockInfo(GLenum aTextureTarget, GLenum aCompressedFormat)
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

        // Sanity check: for compressed format, the internal format is also the "teximage" format
        {
            GLint imageFormat;
            glGetInternalformativ(aTextureTarget, aCompressedFormat,
                                  GL_TEXTURE_IMAGE_FORMAT, 1, &imageFormat);
            // Note: untested while writing, so if it asserts I was wrong in this assumption
            assert(imageFormat == aCompressedFormat);
        }

        return {
            .mIsCompressed = true,
            .mInternalFormat = aCompressedFormat,
            .mTexImageFormat = aCompressedFormat, // For compressed formats, both are the complete format
            .mByteSize = blockByteSize,
            .mDimensions = {(GLsizei)blockWidth, (GLsizei)blockHeight},
        };
    }

    /// @brief Get the image block info for non-compressed formats
    ImageBlockInfo getBlockInfo(GLenum aTextureTarget, GLenum aInternalFormat)
    {
        ImageBlockInfo result{
            .mIsCompressed = false,
            .mInternalFormat = aInternalFormat,
            .mDimensions = {1, 1}, // For non compressed formats, there is no block 
                                   // (i.e. texels are transfered individually)
        };

        glGetInternalformativ(aTextureTarget, aInternalFormat, 
                              GL_IMAGE_TEXEL_SIZE, 1, (GLint*)(&(result.mByteSize)));
        glGetInternalformativ(aTextureTarget, aInternalFormat, 
                              GL_TEXTURE_IMAGE_FORMAT, 1, (GLint*)(&result.mTexImageFormat));
        // Note: here, we rely on the fact that we derived an internal format exactly matching
        // the data type of the image in the DDS:
        // the internal format data type thus indicates the type of the DDS content.
        glGetInternalformativ(aTextureTarget, aInternalFormat, 
                              GL_TEXTURE_IMAGE_TYPE, 1, (GLint*)(&result.mTexImageType));

        return result;
    }

    // TODO: move to a general lower level header
    /// @brief Compute the byte size of an image (i.e. a single level of a texture)
    GLsizei computeImageByteSize(math::Size<2, GLsizei> aImageDimensions,
                                 GLsizei aBlockByteSize,          
                                 math::Size<2, GLsizei> aBlockDimensions)
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
                               const ImageBlockInfo & aBlockInfo,
                               const arte::dds::Header aDdsHeader,
                               std::istream & aDataStream,
                               GLint aLayerIdx = -1 /* -1 implies 2D texture target*/)
    {
        // The image data should be 2D
        assert(aDdsHeader.h.dwDepth == 1);
        assert(!aBlockInfo.mIsCompressed ||
                (aDdsHeader.h_dxt10 
                && aDdsHeader.h_dxt10->resourceDimension == arte::DDS_DIMENSION_TEXTURE2D));

        const GLsizei imageByteSize = 
                computeImageByteSize(aMainImageDimensions, aBlockInfo.mByteSize, aBlockInfo.mDimensions);

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
                if (aBlockInfo.mIsCompressed)
                {
                    glCompressedTexSubImage2D(
                        aLoadedTarget,
                        level,
                        0, 0, // x, y offsets
                        levelDimensions.width(),
                        levelDimensions.height(),
                        aBlockInfo.mTexImageFormat,
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
                        aBlockInfo.mTexImageFormat,
                        aBlockInfo.mTexImageType,
                        imageData.get());
                }
            }
            else
            {
                // TODO: handle 3D equivalent
                assert(aBlockInfo.mIsCompressed);

                // For 3D texture, I am unaware of a use case to make it distinct atm.
                assert(aLoadedTarget == aTexture.mTarget);
                glCompressedTexSubImage3D(aLoadedTarget,
                                          level,
                                          0, 0, aLayerIdx, // x, y, z offsets
                                          levelDimensions.width(),
                                          levelDimensions.height(),
                                          aDdsHeader.h.dwDepth,
                                          aBlockInfo.mTexImageFormat, 
                                          levelByteSize,
                                          imageData.get());
            }

            // Prepare next iteration
            levelDimensions = max((levelDimensions / 2), {1, 1});
            levelByteSize = computeImageByteSize(levelDimensions, aBlockInfo.mByteSize, aBlockInfo.mDimensions);
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

    const GLenum target = graphics::getTextureTarget(header);
    GLenum internalFormat = getTextureFormat(header);
    ImageBlockInfo blockInfo;

    GLint isCompressed;
    glGetInternalformativ(target, internalFormat, 
                          GL_TEXTURE_COMPRESSED, 1, &isCompressed);
    if (isCompressed)
    {
        blockInfo = getCompressedBlockInfo(target, internalFormat);
    }
    else
    {
        blockInfo = getBlockInfo(target, internalFormat);

        // We only tested with this non-compressed format as of writting
        // It should work with the rest, but the first person to test should be aware!
        assert(header.h.ddspf.dwFourCC == 113);

        // Some sanity check we hardcoded while implementing LTC support
        if (header.h.ddspf.dwFourCC == 113)
        {
            assert(target == GL_TEXTURE_2D);
            assert(blockInfo.mByteSize = 8);
            assert(blockInfo.mTexImageFormat == GL_RGBA);
            assert(blockInfo.mTexImageType == GL_HALF_FLOAT);
        }
    }

    graphics::Texture texture{target};
    graphics::ScopedBind boundTexture{texture};
    glTexStorage2D(texture.mTarget, 
                   header.h.dwMipMapCount,
                   blockInfo.mInternalFormat,
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
        
        auto shaderPath = mFinder.pathFor(shaderFile);
        ShaderInclusionLookup lookup{shaderPath, &mFinder};
        shaders.emplace_back(
            stageEnumerator,
            graphics::ShaderSource::Preprocess(std::ifstream{shaderPath},
                                               aDefines,
                                               lookup.top(),
                                               lookup));
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
