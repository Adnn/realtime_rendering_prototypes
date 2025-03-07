#pragma once


#include <engine/Lights.h>

#include <math/Rectangle.h>

#include <reflect/DearImguiWitness.h>
#include <reflect/DescribeMath.h>



namespace ad {


struct alignas(16) CardLight_glsl
{
    GLfloat mHeight;
    bool mDoubleSided = false;
    alignas(8) math::Rectangle<GLfloat> mRect;
    renderer::LightColors_glsl mColors;
};


DESCRIBE(CardLight_glsl)
{
    GIVE(Height);
    GIVE(DoubleSided);
    GIVE(Rect);
    GIVE_EX(aValue.mColors.mDiffuseColor,  "diffuse color");
    GIVE_EX(aValue.mColors.mSpecularColor, "specular color");
}


/// @brief Data that is user controlled, and part of the LightsBlock UBO
struct PlanarLightsBlock
{
    // see: https://registry.khronos.org/OpenGL/specs/gl/glspec45.core.pdf#page=159
    // "If the member is a scalar consuming N basic machine units, the base alignment is N.""
    /*alignas(16)*/ GLuint mPlanarCount{0};

    alignas(16) math::hdr::Rgb<GLfloat> mAmbientColor;
    std::array<CardLight_glsl, renderer::gMaxLights> mPlanarLights;

    //
    // Helpers
    //
    std::span<CardLight_glsl> spanPlanarLights()
    { return std::span{mPlanarLights.data(), mPlanarCount}; }

    std::span<const CardLight_glsl> spanPlanarLights() const
    { return std::span{mPlanarLights.data(), mPlanarCount}; }
};


template <class T_witness>
void describe(T_witness & aW, PlanarLightsBlock & aValue)
{
    give(aW, aValue.mAmbientColor, "ambient color");

    give(aW, Clamped<GLuint>{aValue.mPlanarCount, 0, renderer::gMaxLights}, "planar count");
    give(aW, aValue.spanPlanarLights(), "planar lights");
}


} // namespace ad