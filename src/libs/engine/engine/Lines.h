#pragma once


#include <math/Vector.h>

#include <renderer/GL_Loader.h>

#include <vector>


namespace ad::renderer {


struct LineSegment_glsl
{
    alignas(4 * sizeof(GLfloat)) math::Position<3, GLfloat> mPointA;
    alignas(4 * sizeof(GLfloat)) math::Position<3, GLfloat> mPointB;
    alignas(4 * sizeof(GLfloat)) GLfloat mWidth;
};


struct LinesSsbo_glsl
{
    std::vector<LineSegment_glsl> mSegments;
};


} // namespace ad::renderer