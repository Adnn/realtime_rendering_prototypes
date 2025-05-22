#pragma once

#include "DebugDrawer.h"

#include <engine/IntrospectProgram.h>


namespace ad {
struct Engine;
}

namespace ad::debug {


class DebugRenderer
{
public:
    DebugRenderer(Engine & aEngine);
    void render(const DebugDrawer::DrawList & aDrawList);

private:
    renderer::IntrospectProgram mLineProgram;
    graphics::VertexBufferObject mLineVertices;
    graphics::VertexArrayObject mLinesVao;
};


} // namespace ad::debug
