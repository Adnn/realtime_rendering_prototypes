#pragma once


#include <math/Color.h>
#include <math/Vector.h>

#include <renderer/GL_Loader.h>

#include <vector>


namespace ad::renderer {


struct LineSegment_glsl
{
    alignas(4 * sizeof(GLfloat)) math::Position<3, GLfloat> mPointA;
    alignas(4 * sizeof(GLfloat)) math::Position<3, GLfloat> mPointB;
    alignas(4 * sizeof(GLfloat)) math::hdr::Rgba_f mColor = math::hdr::gWhite<GLfloat>;
    alignas(4 * sizeof(GLfloat)) GLfloat mWidth;
};


struct LinesSsbo_glsl
{
    std::vector<LineSegment_glsl> mSegments;
};


std::vector<math::Position<3, GLfloat>> makeRoundSegment(unsigned int aCircleResolution);


} // namespace ad::renderer