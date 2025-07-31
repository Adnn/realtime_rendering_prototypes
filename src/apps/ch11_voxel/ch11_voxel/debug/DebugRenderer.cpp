#pragma once

#include "DebugRenderer.h"

#include "../Engine.h"

#include "../log/Logging.h"

#include <renderer/BufferLoad.h>


namespace ad::debug {


namespace {

    const renderer::ReferencePath gLineProgram{"programs/DebugDrawLine.prog"};

} // unnamed namespace


DebugRenderer::DebugRenderer(Engine & aEngine) :
    mLineProgram{aEngine.loadProgram(gLineProgram)}
{
    // For creation
    graphics::ScopedBind{mLinesVao};

    // Describe the format of the vertex attribute array
    const GLuint posAttribute = 0;
    const GLuint colorAttribute = 2;
    glEnableVertexArrayAttrib(mLinesVao, posAttribute);
    glVertexArrayAttribFormat(mLinesVao, posAttribute, 3, GL_FLOAT,
                              GL_FALSE, offsetof(LineVertex, mPosition));
    glEnableVertexArrayAttrib(mLinesVao, colorAttribute);
    glVertexArrayAttribFormat(mLinesVao, colorAttribute, 4, GL_FLOAT, 
                              GL_FALSE, offsetof(LineVertex, mColor));

    // Bind the actual vertex buffer to a buffer binding index
    const GLuint bufferBindIdx = 5;
    glVertexArrayVertexBuffer(mLinesVao, bufferBindIdx, mLineVertices, 
                              0, sizeof(LineVertex));

    // Associate the vertex attributes with the buffer binding index
    glVertexArrayAttribBinding(mLinesVao, posAttribute, bufferBindIdx);
    glVertexArrayAttribBinding(mLinesVao, colorAttribute, bufferBindIdx);
}


void DebugRenderer::render(const DebugDrawer::DrawList & aDrawList)
{
    // Load vertex attribute data into the vertex buffer
    graphics::load(mLineVertices,
                   std::span{aDrawList.mLineVertices},
                   graphics::BufferHint::StreamDraw);

    auto scopeVao = graphics::ScopedBind{mLinesVao};
    glUseProgram(mLineProgram);
    glDrawArrays(GL_LINES, 0, aDrawList.mLineVertices.size());
}


} // namespace ad::debug
