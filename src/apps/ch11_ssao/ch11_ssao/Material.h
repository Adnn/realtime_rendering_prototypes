#pragma once


#include <math/Color.h>

#include <reflect/ReflectHelpers.h>

#include <renderer/GL_Loader.h>

#include <array>
#include <span>


namespace ad {


constexpr unsigned int gMaxMaterials = 16;


//
// Phong
//

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


struct PhongMaterialsBlock_glsl
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


DESCRIBE(PhongMaterialsBlock_glsl)
{
    GIVE_EX((Clamped<GLuint>{aValue.mCount, 0, gMaxMaterials}), count);
    GIVE_EX(aValue.spanMaterials(), "materials");
}


//
// (Disney/Epic) PBR
//

struct alignas(16) PbrMaterial_glsl
{
    alignas(4 * sizeof(GLfloat)) math::hdr::Rgba_f mAmbientColor = math::hdr::gWhite<float>;
    alignas(4 * sizeof(GLfloat)) math::hdr::Rgba_f mBaseColor = math::hdr::gWhite<float>;
    alignas(sizeof(GLfloat)) GLfloat mMetallic = 0.f;
    alignas(sizeof(GLfloat)) GLfloat mRoughness = 0.3f;
};


DESCRIBE(PbrMaterial_glsl)
{
    GIVE(AmbientColor);
    GIVE(BaseColor);
    GIVE(Metallic);
    GIVE_EX((Clamped<GLfloat>{aValue.mRoughness, 0.f, 1.f}), Roughness);
}


struct PbrMaterialsBlock_glsl
{
    GLuint mCount{ 0 };
    std::array<PbrMaterial_glsl, gMaxMaterials> mMaterials;

    //
    // Helpers
    //
    std::span<PbrMaterial_glsl> spanMaterials()
    {
        return std::span{ mMaterials.data(), mCount };
    }
};


DESCRIBE(PbrMaterialsBlock_glsl)
{
    GIVE_EX((Clamped<GLuint>{aValue.mCount, 0, gMaxMaterials}), count);
    GIVE_EX(aValue.spanMaterials(), "materials");
}

} // namespace ad
