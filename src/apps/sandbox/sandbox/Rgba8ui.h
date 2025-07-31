#pragma once

#include <renderer/VertexSpecification.h>

#include <engine/IntrospectProgram.h>
#include <engine/Resources.h>

#include <engine/files/Loader.h>


namespace ad::rgba8ui {


renderer::IntrospectProgram loadProgram()
{
    return 
        renderer::Loader{renderer::makeResourceFinder()}
            .loadProgram(renderer::ReferencePath{"programs/sandbox_rgba8ui.prog"});
}


void draw(math::Size<2, int> aRenderResolution)
{
    glViewport(0, 0, aRenderResolution.width(), aRenderResolution.height());

    graphics::VertexArrayObject dummyVao;
    glBindVertexArray(dummyVao);

    renderer::IntrospectProgram program = loadProgram();
    glUseProgram(program);

    const GLsizei gSize = 11;
    const std::size_t gArraySize = gSize * gSize;
    // Fill source data array with a checkerboard
    std::array<math::sdr::Rgba, gArraySize> textureContent;
    for (std::size_t y = 0; y != gSize; ++y)
    {
        for (std::size_t x = 0; x != gSize; ++x)
        {
            textureContent[x + gSize * y] = (x + y) % 2 ?
                math::sdr::gMagenta : (math::sdr::gWhite * 0.2f);
        }
    }

    {
        GLuint texture_1;
        glCreateTextures(GL_TEXTURE_2D, 1, &texture_1);
        glTextureStorage2D(texture_1, 1, GL_RGBA8, gSize, gSize);

        glTextureSubImage2D(texture_1, 0, 0, 0, gSize, gSize,
                            GL_RGBA, GL_UNSIGNED_BYTE, textureContent.data());
        glTextureParameteri(texture_1, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(texture_1, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindTextureUnit(1, texture_1);
    }
    
    {
        GLuint texture_2;
        glCreateTextures(GL_TEXTURE_2D, 1, &texture_2);
        glTextureStorage2D(texture_2, 1, GL_RGBA8UI, gSize, gSize);

        // GL_RGBA_INTEGER is a requirement here, otherwise GL raises an error
        glTextureSubImage2D(texture_2, 0, 0, 0, gSize, gSize,
                            GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, textureContent.data());
        glTextureParameteri(texture_2, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(texture_2, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindTextureUnit(2, texture_2);
    }

    glUniform1i(glGetUniformLocation(program, "u_Texture1"), 1);
    glUniform1i(glGetUniformLocation(program, "u_Texture2"), 2);

    glUniform2i(glGetUniformLocation(program, "u_FramebufferSize"),
                aRenderResolution.width(), aRenderResolution.height());

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}


} // namespace ad::rgba8ui