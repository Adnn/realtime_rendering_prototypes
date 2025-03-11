#pragma once


#include <engine/Lights.h>

#include <math/Rectangle.h>

#include <reflect/DearImguiWitness.h>
#include <reflect/DescribeMath.h>



namespace ad {


// std140 rule 9.: struct is aligned to its largest member alignment, rounded up to 16 
struct alignas(16) CardLight_glsl
{
    // std140 rule 1.: natural alignment of 4 is correct
    GLfloat mHeight;

    // Important: we manually pad with 3 zero initialized bytes,
    // because the GLSL bool type is 4-bytes wide.
    // If we simply used alignas here in the cpp struct, 
    // the garbage in padding would likely always make the bool true.
    static_assert(sizeof(bool) == 1);
    // TODO: In production, we would pack those bools
    char mPadDouble[3] = { 0 };
    bool mDoubleSided = false;
    char mPadClip[3] = { 0 };
    bool mClipHorizon = false;

    // Note: in GLSL, we do not have struct matching the rect, but directly read it as 2x vec2
    // std140 rule 2.: alignment of vec2 is 2x4.
    alignas(8) math::Rectangle<GLfloat> mRect;
    // Note: this member is actually matched to a GLSL struct.
    // We rely on LightColors_glsl struct to define its alignment
    renderer::LightColors_glsl mColors;
};


DESCRIBE(CardLight_glsl)
{
    GIVE(Height);
    GIVE(DoubleSided);
    GIVE(ClipHorizon);
    GIVE(Rect);
    GIVE_EX(aValue.mColors.mDiffuseColor,  "diffuse color");
    GIVE_EX(aValue.mColors.mSpecularColor, "specular color");
}


// TODO: can we implement a witness for this functionnality?
inline std::ostream& getCardlightLayout(std::ostream & aOut)
{
    return aOut <<
        "Height: " << offsetof(CardLight_glsl, mHeight) << ", "
        "DoubleSided: " << offsetof(CardLight_glsl, mPadDouble) << ", "
        "ClipHorizon: " << offsetof(CardLight_glsl, mPadClip) << ", "
        "Rect: " << offsetof(CardLight_glsl, mRect) << ", "
        "Colors: " << offsetof(CardLight_glsl, mColors) << ", "
    ;
}

/// @brief Data that is user controlled, and part of the LightsBlock UBO
struct PlanarLightsBlock
{
    // see: https://registry.khronos.org/OpenGL/specs/gl/glspec45.core.pdf#page=159
    // "If the member is a scalar consuming N basic machine units, the base alignment is N.""
    alignas(4) GLuint mPlanarCount{0};

    // Matched to a vec4 in GLSL
    alignas(16) math::hdr::Rgb<GLfloat> mAmbientColor;
    // std140 rule 10.: Each element in an array of struct is laid out according to the struct alignment
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