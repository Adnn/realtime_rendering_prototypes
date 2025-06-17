#pragma once


#include "Hierarchy.h"
#include "Material.h"
#include "Pose.h"

#include <engine/ColorSpace.h>
#include <engine/Semantic.h>

#include <math/Box.h>

#include <renderer/BufferBase.h>
#include <renderer/Texture.h>
#include <renderer/VertexSpecification.h>

#include <unordered_map>
#include <vector>


namespace ad::scenic {

template <class T>
class Handle
{
    using Index_t = std::vector<std::remove_cv_t<T>>::size_type;
public:
    void operator=(Index_t aValue)
    { mIndex = aValue; }

    Index_t mIndex;
};


// Or just the GLint name directly?
template <>
class Handle<const graphics::BufferAny> : public graphics::Name<graphics::BufferAny>
{
public:
    graphics::Name<graphics::BufferAny> operator*() const
    { return *this; }
};

// Note: Represent an "homogeneous" chunck in a buffer, for example an array of per-vertex or per-instance data
// (with a shared stride and instance divisor).
// (NB: A buffer can thus store heterogenous data, each addressed by distinct views)
// If we allow to store distinct logical objects in the same view, we can then use the view (or VertexStream) 
// as an identifier to match VAOs (while batching).
// This implies that a Part will need an offset into its buffer views.
// This offset in implemented by the Part m*First and m*Count members.
struct BufferView
{
    Handle<const graphics::BufferAny> mGLBuffer;
    // The stride is at the view level (not buffer).
    // This way the buffer can store heterogeneous sections, accessed by distinct views.
    GLsizei mStride;
    // The instance divisor it at the view level, not AttributeAccessor
    // The reasoning is that since the view is supposed to represent an homogeneous chunck of the buffer,
    // the divisor (like the stride) will be shared by all vertices pulled from the view.
    GLuint mInstanceDivisor = 0;
    GLintptr mOffset; // Offset of this View into the GL Buffer.
    GLsizeiptr mSize; // The size (in bytes) this buffer view has access to, starting from mOffset.
                      // Intended to be used for tooling and safety checks.
};

/// @brief Accessor for a single attribute (usually mapping to a semantic) into a BufferView
///
/// Like with Gltf, this adds typing on top of BufferView, but it does handle the global 
/// offset into the view, which derived from MeshPart data members.
struct AttributeAccessor
{
    std::vector<BufferView>::size_type mBufferViewIndex;
    graphics::ClientAttribute mClientDataFormat;
};


// TODO: should we dod the BufferViews? 
#define SEMANTIC_BUFFER_MEMBERS \
std::vector<BufferView> mVertexBufferViews; \
std::unordered_map<renderer::Semantic, AttributeAccessor> mSemanticToAttribute;

///// \note Intermediary class that holds a collection of buffer views,
///// with associated semantic and client format description.
//struct GenericStream
//{
//    SEMANTIC_BUFFER_MEMBERS                 
//};


// Note: Not using inheritance, since it does allow designated initializers
// Important: Used as the lookup key for ProgramConfig VAOs
struct VertexStream /*: public SemanticBufferViews*/
{
    SEMANTIC_BUFFER_MEMBERS                 
    BufferView mIndexBufferView;
    GLenum mIndicesType = GL_NONE;
};

// TODO: should we DoD?
struct MeshPart
{
    Handle<const VertexStream> mVertexStream;
    GLenum mPrimitiveMode = 0;
    // GLuint because it is the type used in the Draw Indirect buffer
    GLuint mVertexFirst = 0; // The offset (as a count of vertices) of this part into the vertex attributes buffer view(s)
                             // (This is the basevertex in OpenGL lingua)
    GLuint mVertexCount = 0;
    GLuint mIndexFirst = 0; // The offset (as a count of indices) of this part indices into the index buffer view
    GLuint mIndicesCount = 0;   

    math::Box<float> mAabb;
};


struct Material
{
    Handle<const GenericMaterial_glsl> mSurfaceParameters;
};

// Note: A representation of a mesh that owns an individual buffer for each vertex attribute
//       This is simpler to implement (and manage lifetimes) than a shared buffer approach
//       At the cost of leading to a distinct VAO per mesh (so state change, plus distinct draw calls).
struct MeshPart_Naive
{
    struct Accessor_Naive
    {
        graphics::BufferAny mBuffer;
        graphics::ClientAttribute mClientDataFormat{
            // Note: Make Accessor_Naive default ctible, for serialization
            .mDimension = {0},
        };
    };

    std::unordered_map<renderer::Semantic, Accessor_Naive> mSemanticToAttribute;
    graphics::BufferAny mIndexBuffer;
    GLenum mIndicesType = GL_NONE;

    GLenum mPrimitiveMode = 0;
    // GLuint because it is the type used in the Draw Indirect buffer
    GLuint mVertexFirst = 0; // The offset (as a count of vertices) of this part into the vertex attributes buffer view(s)
                             // (This is the basevertex in OpenGL lingua)
    GLuint mVertexCount = 0;
    GLuint mIndexFirst = 0; // The offset (as a count of indices) of this part indices into the index buffer view
    GLuint mIndicesCount = 0;   

    math::Box<float> mAabb;

    Material mMaterial;
};


/// @return `true` if the Mesh use indices for vertex attributes (`glDrawElement()`)
/// `false` otherwise (`glDrawArray()`).
inline bool useElementIndices(const MeshPart_Naive & aMesh)
{ 
    return aMesh.mIndicesType != GL_NONE; 
}


// TODO: find a "less generic" name, such as Assembly
// Gltf call it "Mesh" (and calls "MeshPrimitive" for the atomic Mesh/Part)
// Assimp does not seem to have explicit groups of aiMesh: they are directly listed under an aiNode
/// @brief 
struct Object
{
    std::vector<MeshPart_Naive> mParts;
    math::Box<float> mAabb;
};


// TODO: can we actually implement a scene graph?
struct SceneTree
{
    NodeTree<Pose> mTree;
    // TODO: AABB, union of own Object's AABB and all children Nodes AABBs
    // std::vector<AABB> mAABBs
    std::unordered_map<Node::Index, Object> mObjectsMap;
};


math::Box<float> getAabb(const SceneTree & aSceneTree);


SceneTree & mergeScenes(SceneTree & aBaseTree,
                        SceneTree & aMerged,
                        Node::Index aParent = Node::gInvalidIndex);


using TexturePaths = std::vector<std::pair<std::string, renderer::ColorSpace>>;
// TODO: rename, this is more general than models
struct ModelStorage
{
    //std::vector<VertexStream> mVertexStreams;
    GenericMaterialsBlock_glsl mMaterials;

    std::vector<graphics::Texture> mTextures;
    TexturePaths mTexturePaths;
};


inline const GenericMaterial_glsl & get(const ModelStorage & aStorage, Handle<const GenericMaterial_glsl> aHandle)
{
    assert(aHandle.mIndex < aStorage.mMaterials.mCount);
    return aStorage.mMaterials.mMaterials[aHandle.mIndex];
}


} // namespce ad::scenic
