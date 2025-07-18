#pragma once


#include <math/Color.h>

#include <reflect/ReflectHelpers.h>

#include <renderer/GL_Loader.h>

#include <array>
#include <span>


namespace ad::scenic {


constexpr unsigned int gMaxMaterials = 128;


struct TextureInput
{
    using Index = unsigned int;
    inline static constexpr Index gNoEntry = std::numeric_limits<Index>::max();

    // NOTE Ad 2024/02/21: If one day we go with bindless textures, this should probably become
    //   the "address" of a plain TEXTURE_2D 
    //   (since we would not have to assemble several logical textures in a common TEXTURE_2D_ARRAY)
    Index mTextureIndex = gNoEntry; // Index of the texture in the TEXTURE_2D_ARRAY.

    Index mUVAttributeIndex = gNoEntry;
};


/// \brief Attempts to cover both Phong and Pbr materials, to simplify development
struct alignas(16) GenericMaterial_glsl
{
    // 4 components, so it can be loaded directly on the GPU without alignment issues.
    math::hdr::Rgba<float> mAmbientColor  = math::hdr::gWhite<float>;
    math::hdr::Rgba<float> mDiffuseColor  = math::hdr::gWhite<float>;
    math::hdr::Rgba<float> mSpecularColor = math::hdr::gWhite<float>;
    TextureInput mDiffuseMap; 
    TextureInput mNormalMap; 
    TextureInput mMetallicRoughnessAoMap; 
    float mSpecularExponent = 1.f;
};


DESCRIBE(GenericMaterial_glsl)
{
    GIVE(AmbientColor);
    GIVE(DiffuseColor);
    GIVE(SpecularColor);
    GIVE(SpecularExponent);
}


struct GenericMaterialsBlock_glsl
{
    GLuint mCount{ 0 };
    std::array<GenericMaterial_glsl, gMaxMaterials> mMaterials;

    //
    // Helpers
    //
    std::span<GenericMaterial_glsl> spanMaterials()
    {
        return std::span{ mMaterials.data(), mCount };
    }
};


template <class T_witness>
void describe(T_witness & aWitness,
              GenericMaterialsBlock_glsl & aValue,
              std::span<std::string> aNames)
{
    GIVE_EX((Clamped<GLuint>{aValue.mCount, 0, gMaxMaterials}), "count");
    GIVE_EX(aValue.spanMaterials(), aNames);
}


} // namespace ad::scenic
