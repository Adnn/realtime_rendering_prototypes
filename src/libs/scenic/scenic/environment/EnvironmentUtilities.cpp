#include "EnvironmentUtilities.h"

#include "Skybox.h"

#include "../Camera.h"
#include "../log/Logging.h"

#include <engine/files/Loader.h>

#include <graphics/CameraUtilities.h>

#include <math/Transformations.h>

#include <renderer/BufferLoad.h>
#include <renderer/FrameBuffer.h>
#include <renderer/UniformBuffer.h>
#include <renderer/Uniforms.h>

#include <cassert>

namespace ad::scenic {

namespace {

    constexpr auto gNegateVertical = math::trans3d::scale(1.f, -1.f, 1.f);

    // Note: OpenGL order of cubemap faces is +X, -X, +Y, -Y, +Z, -Z
    // The coordinate system of the cubemap is **left-handed**,
    // thus a camera facing -Z in our right-handed world should render the cubemap +Z face.
    // (see: https://www.khronos.org/opengl/wiki/Cubemap_Texture#Upload_and_orientation)
    // Important: The camera vertical axis is negated (not rotated!) in order to generate images with a top-left origin,
    // (i.e. first bytes appearing in the texture correspond to the top row, instead of the bottom row)
    // since cubemaps, unlike all other OpenGL textures, are behaving as having a top-left origin.
    // (Apparently, this is coming from Renderman,)
    const std::array<math::AffineMatrix<4, GLfloat>, 6> gCubeCaptureViewsNegateY{
        graphics::getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, { 1.f,  0.f,  0.f}) * gNegateVertical,
        graphics::getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, {-1.f,  0.f,  0.f}) * gNegateVertical,
        graphics::getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, { 0.f,  1.f,  0.f}, {0.f,  0.f,  1.f}) * gNegateVertical,
        graphics::getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, { 0.f, -1.f,  0.f}, {0.f,  0.f, -1.f}) * gNegateVertical,
        graphics::getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, { 0.f,  0.f, -1.f}) * gNegateVertical, // -Z in our right handed basis
        graphics::getCameraTransform<GLfloat>({0.f, 0.f, 0.f}, { 0.f,  0.f,  1.f}) * gNegateVertical,
    };

    void setupCubeFiltering(const graphics::Texture & aCubemap)
    {
        glTextureParameteri(aCubemap, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(aCubemap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameterf(aCubemap, GL_TEXTURE_MAX_ANISOTROPY, 16.f);
    }


    graphics::Texture prepareCubemap(const EnvironmentMap & aEnvironment, math::Size<2, GLsizei> aSize, GLsizei aLevelsCount)
    {
        // We could actually hande equirectangular, but it will also require extension of the filtering shader
        assert(aEnvironment.mType == EnvironmentMap::Type::Cubemap);
        
        // Get the internal format of the provided environment texture
        GLint environmentInternalFormat = 0;
        {
            graphics::ScopedBind boundEnvironment{aEnvironment.mTexture};
            glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                                     0,
                                     GL_TEXTURE_INTERNAL_FORMAT,
                                     &environmentInternalFormat);
            
            // Special care has to be taken if the internal format is compressed
            switch(environmentInternalFormat)
            {
                default:
                {
                    GLint isCompressed = GL_TRUE;
                    glGetInternalformativ(aEnvironment.mTexture, environmentInternalFormat, 
                                          GL_TEXTURE_COMPRESSED, 1, &isCompressed);
                    if(isCompressed)
                    {
                        // TODO complete the mapping if you end-up here
                        ADLOG(critical)("Environment map internal format {} is compressed and requires manual mapping.",
                                        graphics::to_string(environmentInternalFormat));
                        throw std::domain_error{"Compressed texture format unmapped for renderable cubemap."};
                    }
                    break;
                }
                case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
                {
                    // It seems it is mapping to 16 bits per channel when decompressed
                    environmentInternalFormat = GL_RGB16F;
                    break;
                }
            }
        }

        graphics::Texture cubemap{GL_TEXTURE_CUBE_MAP};
        graphics::allocateStorage(cubemap,
                                  environmentInternalFormat,
                                  aSize.width(), aSize.height(),
                                  aLevelsCount);

        return cubemap;
    }
        

    void renderCubemapFaces(const renderer::IntrospectProgram & aProgram,
                            const EnvironmentMap & aEnvMap,
                            const graphics::Texture & aTargetCubemap,
                            GLsizei aLevel)
    {
        // TODO implement as layered rendering instead
        // see: https://www.khronos.org/opengl/wiki/Geometry_Shader#Layered_rendering

        Camera orthographicFace;
        // TODO: #resources This UBO could be cached
        graphics::UniformBufferObject viewProjectionUbo;

        constexpr unsigned int gFaceCount = std::size(gCubeCaptureViewsNegateY);
        for(unsigned int faceIdx = 0; faceIdx != gFaceCount; ++faceIdx)
        {
            // We attach the current texture level to the Framebuffer's draw color buffer attachment 1 
            // (it could be zero since we do not attach to another color buffer, this is just to be fancy)
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT1,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIdx, 
                                   aTargetCubemap, 
                                   aLevel);

            // Map output fragment color at location 0 to the draw buffer at color attachment 1
            // (this color attachment was set to the output texture face just above)
            glDrawBuffer(GL_COLOR_ATTACHMENT1);

            // Set the appropriate camera pose for this face's pass
            // BUGFIXED: It is required to write the destination cubemap images as having top-left origin
            // This is implemented by negating the Y coordinate of gl_Position (via the provided camera transform)
            orthographicFace.setPose(gCubeCaptureViewsNegateY[faceIdx]);
            graphics::loadSingle(viewProjectionUbo,
                                 GpuViewProjectionBlock{ orthographicFace },
                                 graphics::BufferHint::StreamDraw);
            graphics::ScopedBind boundCameraUbo{viewProjectionUbo, graphics::BindingIndex{ 0 } };
            glClear(GL_COLOR_BUFFER_BIT);

            // We need to render cube inner-faces (backfaces), but they are turned into frontfaces by the negated Y camera
            passSkyboxBase(aProgram, aEnvMap, GL_BACK);
        }
    }

} // unnamed namespace


graphics::Texture loadCubemapFromDds(filesystem::path aDds)
{
    graphics::Texture cubemap = renderer::loadDds(aDds);
    assert(cubemap.mTarget == GL_TEXTURE_CUBE_MAP);
    setupCubeFiltering(cubemap);
    return cubemap;
}


graphics::Texture filterEnvironmentMapDiffuse(const EnvironmentMap& aEnvMap,
                                              GLsizei aOutputSideLength,
                                              renderer::Loader & aLoader)
{
    PROFILER_SCOPE_SINGLESHOT_SECTION(gRenderProfiler, "filter env: diffuse irradiance", CpuTime, GpuTime);

    // Texture level 0 (maximum) size
    const math::Size<2, GLsizei> size{aOutputSideLength, aOutputSideLength};
    constexpr GLint textureLevels = 1;
    graphics::Texture filteredCubemap = prepareCubemap(aEnvMap, size, textureLevels);

    graphics::FrameBuffer framebuffer;
    graphics::ScopedBind boundFbo{framebuffer, graphics::FrameBufferTarget::Draw};

    // TODO: #resources we should not have to recompile on each invocation
    // The question is wether we want to rely on a general caching system (that should be low-level enough)
    // or if we go the way of making this a member function, and hosting a copy in the data members.
    renderer::IntrospectProgram program =
        aLoader.loadProgram(renderer::ReferencePath{ "programs/PrefilterCubemap.prog" },
                            { "DIFFUSE_IRRADIANCE", });

    glViewport(0, 0, size.width(), size.height());

    constexpr GLint level = 0;
    renderCubemapFaces(program, aEnvMap, filteredCubemap, level);

    glTextureParameteri(filteredCubemap, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(filteredCubemap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glObjectLabel(GL_TEXTURE, filteredCubemap, -1, "filtered_irradiance_diffuse_env");
    return filteredCubemap;
}


graphics::Texture filterEnvironmentMapGgxSpecular(const EnvironmentMap& aEnvMap,
                                               GLsizei aOutputSideLength,
                                               renderer::Loader & aLoader)
{
    PROFILER_SCOPE_SINGLESHOT_SECTION(gRenderProfiler, "filter env: specular radiance", CpuTime, GpuTime);

    // Texture level 0 (maximum) size
    const math::Size<2, GLsizei> size{aOutputSideLength, aOutputSideLength};
    const GLint textureLevels = graphics::countCompleteMipmaps(size);
    assert(textureLevels > 1); // otherwise there is just one value for roughness (zero), which is likely wrong
    graphics::Texture filteredCubemap = prepareCubemap(aEnvMap, size, textureLevels);

    graphics::FrameBuffer framebuffer;
    graphics::ScopedBind boundFbo{framebuffer, graphics::FrameBufferTarget::Draw};

    // TODO: #resources we should not have to recompile on each invocation
    // The question is wether we want to rely on a general caching system (that should be low-level enough)
    // or if we go the way of making this a member function, and hosting a copy in the data members.
    renderer::IntrospectProgram program =
        aLoader.loadProgram(renderer::ReferencePath{ "programs/PrefilterCubemap.prog" },
                            { "SPECULAR_RADIANCE", });

    math::Size<2, GLsizei> levelSize = size;
    for(GLint level = 0; level != textureLevels; ++level)
    {
        glViewport(0, 0, levelSize.width(), levelSize.height());

        // Roughness zero seems wasteful (I suppose it should be identical to the unfiltered cubemap)
        // but probably more correct to allow mip-levels interpolation.
        float roughness = (float)level / (textureLevels - 1); // Note: we asserted that textueLevels is more than 1
        // TODO: find a more dynamic way to bind those plain uniforms
        graphics::setUniform(program, "u_Roughness", roughness);

        renderCubemapFaces(program, aEnvMap, filteredCubemap, level);

        // This is the mipmap size derivation described in: 
        // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexStorage2D.xhtml
        levelSize = max((levelSize / 2), {1, 1});
    }
    
    // TODO #bug: Despite the skybox being rendered as SEAMLESS_CUBEMAP (see passSkyboxBase),
    // the seems are visible at transitions between LODs. 
    // Since mipmap filtering is set to linear, I suppose this means there is a discontinuity
    // in the computed LOD at skybox edges.
    // Note: I could confirm that the filtered mipmaps do not have seems by sampling with textureLod()
    glTextureParameteri(filteredCubemap, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(filteredCubemap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Probably useless since we select the LOD explicitly based on roughness
    //glTextureParameterf(filteredCubemap, GL_TEXTURE_MAX_ANISOTROPY, 16.f);

    glObjectLabel(GL_TEXTURE, filteredCubemap, -1, "filtered_radiance_specular_env");
    return filteredCubemap;
}


graphics::Texture integrateEnvironmentBrdf(GLsizei aOutputSideLength,
                                           renderer::Loader& aLoader)
{
    PROFILER_SCOPE_SINGLESHOT_SECTION(gRenderProfiler, "integrate environment brdf", CpuTime, GpuTime);

    const math::Size<2, GLsizei> size{aOutputSideLength, aOutputSideLength};

    graphics::Texture result{GL_TEXTURE_2D};
    graphics::allocateStorage(result,
                              // Note: the paper recommended 16bit floats for precision
                              GL_RG32F, // This is the commonly used internal format around these functions
                              size.width(), size.height(),
                              1);

    graphics::FrameBuffer framebuffer;
    graphics::ScopedBind boundFbo{framebuffer, graphics::FrameBufferTarget::Draw};

    // We attach the current texture level 0 to the Framebuffer's draw color buffer attachment 1 
    // (it could be zero since we do not attach to another color buffer, this is just to be fancy)
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                            GL_COLOR_ATTACHMENT1,
                            GL_TEXTURE_2D,
                            result, 
                            0);

    // Map output fragment color at location 2 to the draw buffer at color attachment 1
    // (once again, just to be fancy, we could use the default mapping 
    //  of fragment color at location 0 to the draw buffer at color attachment 0)
    static const std::array<GLenum, 3> drawBuffers{GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT1};
    glDrawBuffers((GLsizei)drawBuffers.size(), drawBuffers.data());

    glViewport(0, 0, size.width(), size.height());

    glClear(GL_COLOR_BUFFER_BIT);

    renderer::IntrospectProgram program =
        aLoader.loadProgram(renderer::ReferencePath{ "programs/IntegrateEnvironmentBrdf.prog" });
    graphics::ScopedBind boundProgram(program);

    graphics::VertexArrayObject dummyVao;
    graphics::ScopedBind boundVao{ dummyVao };
    // Draw the fullscreen quad (which will invoke the FS for each ouptut pixel of the viewport)
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glTextureParameteri(result, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(result, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTextureParameteri(result, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(result, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glObjectLabel(GL_TEXTURE, result, -1, "integrated_env_brdf");
    return result;
}


} // namespce ad::scenic