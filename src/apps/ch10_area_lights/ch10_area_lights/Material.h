#pragma once


#include <math/Color.h>

#include <reflect/ReflectHelpers.h>

#include <renderer/GL_Loader.h>

#include <array>
#include <span>


namespace ad {


constexpr unsigned int gMaxMaterials = 16;


struct alignas(16) PhongMaterial_glsl
{
	alignas(sizeof(GLfloat)) GLfloat mSpecularExponent = 10.f;
    alignas(4 * sizeof(GLfloat)) math::hdr::Rgba_f mAmbientColor  = math::hdr::gWhite<float>;
    alignas(4 * sizeof(GLfloat)) math::hdr::Rgba_f mDiffuseColor  = math::hdr::gWhite<float>;
    alignas(4 * sizeof(GLfloat)) math::hdr::Rgba_f mSpecularColor = math::hdr::gWhite<float>;
};


DESCRIBE(PhongMaterial_glsl)
{
    GIVE(SpecularExponent);
    GIVE(AmbientColor);
    GIVE(DiffuseColor);
    GIVE(SpecularColor);
}


struct MaterialsBlock_glsl
{
    GLuint mCount{ 0 };
    std::array<PhongMaterial_glsl, gMaxMaterials> mMaterials;

    //
    // Helpers
    //
    std::span<PhongMaterial_glsl> spanMaterials()
    {
        return std::span{ mMaterials.data(), mCount };
    }
};


DESCRIBE(MaterialsBlock_glsl)
{
    GIVE_EX((Clamped<GLuint>{aValue.mCount, 0, gMaxMaterials}), count);
    GIVE_EX(aValue.spanMaterials(), "materials");
}

} // namespace ad
