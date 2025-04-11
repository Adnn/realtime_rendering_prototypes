#pragma once


#include <math/Color.h>
#include <math/Homogeneous.h>

#include <reflect/ReflectHelpers.h>

#include <renderer/GL_Loader.h>

#include <vector>


namespace ad::renderer {


/// @brief Model for the per simulation-entity data that has to be available in shaders.
///
/// An entity is not matching the notion of GL instance (since a logic entity might contains several Parts).
/// So this cannot be made available as instance attributes, but should probably be accessed via buffer-backed blocks.
struct alignas(16) EntityData_glsl
{
    // Note: 16-aligned, because it is intended to be stored as an array in a buffer object
    // and then the elements are accessed via a std140 uniform block

    /*alignas(16)*/ math::AffineMatrix<4, GLfloat> mLocalToWorld = 
        math::AffineMatrix<4, GLfloat>::Identity();
    /*alignas(16)*/ math::hdr::Rgba_f mColorFactor = math::hdr::gWhite<GLfloat>;
};


// TODO: this is too dangerous: if this block was loaded directly, it would load the vector stack object
// not the actual data
struct EntitiesBlock_glsl
{
    std::vector<EntityData_glsl> mEntities;
};


// TODO: do we want this model?
/// @brief SoA on the number of entities, comprising the EntitiesData to be uploaded as uniform buffer
/// and data to be provded as instance attribute.
//struct EntitiesRecord
//{
//    void append(SnacGraph::EntityData_glsl aEntityData, GLuint aMatrixPaletteOffset)
//    {
//        mEntitiesBlock.mEntities.push_back(std::move(aEntityData));
//        mMatrixPaletteOffsets.push_back(aMatrixPaletteOffset);
//    }
//
//    std::size_t size() const
//    {
//        assert (mEntitiesBlock.mEntities.size() == mMatrixPaletteOffsets.size());
//        return mEntitiesBlock.mEntities.size();
//    }
//
//    // To be uploaded as UBO
//    EntitiesBlock_glsl mEntitiesBlock;
//    // To populate instance attributes
//    std::vector<GLuint> mMatrixPaletteOffsets; // offset to the first joint of this instance in the buffer of joints.
//}


} // namespace ad::renderer
