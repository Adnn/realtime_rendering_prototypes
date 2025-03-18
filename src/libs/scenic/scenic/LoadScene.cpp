#include "LoadScene.h"

#include "AssimpUtils.h"
#include "VertexStreamUtilities.h"

#include "log/Logging.h"

#include <engine/SemanticValues.h>

#include <math/Box.h>

#include <assimp/DefaultLogger.hpp> 
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <fmt/std.h>

#include <set>

#define VERBOSE_ASSIMP


namespace ad::scenic {


namespace {

    using IndexType = std::remove_pointer_t<decltype(std::declval<aiFace>().mIndices)>;
    static_assert(std::is_same_v<IndexType, unsigned int>);


    /// @brief Return type on "visiting" a node (i.e. recurseNode())
    struct NodeResult
    {
        //math::Box<float> mAabb;
        unsigned int mVerticesCount = 0;
        unsigned int mIndicesCount = 0;

        bool operator==(const NodeResult&) const = default;
    };


    using MeshMap = std::set<unsigned int>;

    //struct BufferMap
    //{
    //    struct Data
    //    {
    //        Handle<const VertexStream> mStream;
    //        GLsizei mPutPosition;
    //    };
    //    std::unordered_map<
    //        std::set<renderer::Semantic>,
    //        Data> mStreamFromSemantics;
    //};


    MeshPart_Naive handleMesh(aiMesh * aMesh)
    {
        aiVector3D * normals = aMesh->mNormals;
        // Should always be present, we request smooth normals on load
        assert(normals);
        aiVector3D * tangents = aMesh->mTangents;
        // Note that with assimp, tangents and bitangents are either both present or both absent.
        aiVector3D * bitangents = aMesh->mBitangents;

        assert(bitangents || !tangents); // Note: we should never fail here: if tangents were presents,
                                         // assimp guarantees bitangents are present

        // TODO: factorize
        MeshPart_Naive mesh{
            .mIndicesType = graphics::MappedGL_v<IndexType>,
            .mPrimitiveMode = GL_TRIANGLES,
            .mVertexFirst = 0, // buffer is used for this mesh only
            .mVertexCount = aMesh->mNumVertices,
            .mIndexFirst = 0, // index buffer is used for this mesh only
            .mIndicesCount = aMesh->mNumFaces * 3,
            .mAabb = extractAabb(aMesh),
        };

        // Vertices positions
        {
            AttributeDescription attribute{
                .mSemantic = renderer::semantic::gPosition,
                .mDimension = 3,
                .mComponentType = GL_FLOAT,
            };

            mesh.mSemanticToAttribute.insert(
                makeLoadedAccessor_Naive(attribute,
                                         std::span{ aMesh->mVertices, aMesh->mNumVertices },
                                         GL_STATIC_DRAW));
        }

        // Vertices normals
        {
            AttributeDescription attribute{
                .mSemantic = renderer::semantic::gNormal,
                .mDimension = 3,
                .mComponentType = GL_FLOAT,
            };

            mesh.mSemanticToAttribute.insert(
                makeLoadedAccessor_Naive(attribute,
                                         std::span{ aMesh->mNormals, aMesh->mNumVertices },
                                         GL_STATIC_DRAW));
        }
        // Indices
        mesh.mIndexBuffer = makeBuffer(sizeof(IndexType),
                                       mesh.mIndicesCount,
                                       GL_STATIC_DRAW);
        std::unique_ptr<IndexType[]> indexBuffer{ new IndexType[mesh.mIndicesCount] };
        for(std::size_t faceIdx = 0; faceIdx != aMesh->mNumFaces; ++faceIdx)
        {
            const aiFace & face = aMesh->mFaces[faceIdx];
            assert(face.mNumIndices == 3);
            std::memcpy(indexBuffer.get() + (faceIdx * 3), face.mIndices, sizeof(IndexType) * 3);
        }
        graphics::replaceSubset(mesh.mIndexBuffer, 0, std::span{ indexBuffer.get(), mesh.mIndicesCount });

#if 0
        // Vertices
        mArchive.write(aMesh->mNumVertices);
        mArchive.write(std::span{aMesh->mVertices, aMesh->mNumVertices});

        assert(aMesh->mNormals != nullptr);
        mArchive.write(std::span{aMesh->mNormals, aMesh->mNumVertices});

        if(tangents)
        {
            mArchive.write(std::span{tangents, aMesh->mNumVertices});
        }
        if(bitangents)
        {
            mArchive.write(std::span{bitangents, aMesh->mNumVertices});
        }

        mArchive.write(aMesh->GetNumColorChannels());
        for (unsigned int colorIdx = 0; colorIdx != aMesh->GetNumColorChannels(); ++colorIdx)
        {
            mArchive.write(std::span{aMesh->mColors[colorIdx], aMesh->mNumVertices});
        }

        mArchive.write(aMesh->GetNumUVChannels());
        for (unsigned int uvIdx = 0; uvIdx != aMesh->GetNumUVChannels(); ++uvIdx)
        {
            // Only support bidimensionnal texture sampling atm.
            // Sadly, some models have a 3 here, even if they actually use only 2 (e.g. teapot.obj)
            //assert(aMesh->mNumUVComponents[uvIdx] == 2);
            // TODO Interleave the odd channel with their preceding even channel
            // (i.e. better usage of the 4 components of each vertex attribute)
            for(unsigned int vertexIdx = 0; vertexIdx != aMesh->mNumVertices; ++vertexIdx)
            {
                mArchive.write(std::span{&(aMesh->mTextureCoords[uvIdx][vertexIdx].x), 2});
                // Even the assertion below does not hold true for teapot.obj
                //assert(aMesh->mTextureCoords[uvIdx][vertexIdx].z == 0);
            }
        }

        // Faces
        mArchive.write(aMesh->mNumFaces);
        for(std::size_t faceIdx = 0; faceIdx != aMesh->mNumFaces; ++faceIdx)
        {
            const aiFace & face = aMesh->mFaces[faceIdx];
            mArchive.write(std::span{face.mIndices, 3});
        }
#endif
        return mesh;
    }

    NodeResult countVerticesRecurse(aiNode * aNode,
                             const aiScene * aScene,
                             unsigned int aLevel = 0)
    {
        NodeResult result;

        for(std::size_t meshIdx = 0; meshIdx != aNode->mNumMeshes; ++meshIdx)
        {
            unsigned int globalMeshIndex = aNode->mMeshes[meshIdx];
            aiMesh * mesh = aScene->mMeshes[globalMeshIndex];
            result.mVerticesCount += mesh->mNumVertices;
            // We requested triangles (we could also double check by iterating the faces)
            result.mIndicesCount += mesh->mNumFaces * 3;
        }
        for(std::size_t childIdx = 0; childIdx != aNode->mNumChildren; ++childIdx)
        {
            NodeResult childResult = 
                countVerticesRecurse(aNode->mChildren[childIdx], aScene, aLevel + 1);

            // Accumulate total into result
            result.mVerticesCount += childResult.mVerticesCount;
            result.mIndicesCount  += childResult.mIndicesCount;
        }

        return result;
    }

    NodeResult countVerticesDirect(aiNode* aNode, const aiScene* aScene)
    {
        // TODO: Note can we directly sum on all meshes present in the file instead?
        NodeResult result;
        for (std::size_t meshIdx = 0; meshIdx != aScene->mNumMeshes; ++meshIdx)
        {
            aiMesh* mesh = aScene->mMeshes[meshIdx];
            result.mVerticesCount += mesh->mNumVertices;
            // We requested triangles (we could also double check by iterating the faces)
            result.mIndicesCount += mesh->mNumFaces * 3;
        }
        return result;
    }


    NodeResult recurseNodes(aiNode* aNode,
        const aiScene* aScene,
        SceneTree& aOutScene,
        MeshMap& aMeshMap,
        Node::Index aParent,
        unsigned int aLevel = 0)
    {
        std::cout << std::string(2 * aLevel, ' ') << "'" << aNode->mName.C_Str() << "'"
            << ", " << aNode->mNumMeshes << " mesh(es)"
            << ", " << aNode->mNumChildren << " child(ren)"
            << "\n"
            ;
        //ADLOG(debug)("\n{}'{}', {} mesh(es), {} child(ren)",
        //             std::string(2 * aLevel, ' '), aNode->mName.C_Str(), aNode->mNumMeshes, aNode->mNumChildren);

        NodeResult result;
        //// Prime this node's bounding box
        //if(aNode->mNumMeshes != 0)
        //{
        //    result.mAabb = extractAabb(aScene->mMeshes[0]);
        //}
        Node::Index thisIndex =
            aOutScene.mTree.addNode(aParent, decompose(extractAffinePart(aNode)));

        // NOTE Ad 2024/02/28: This is not a hard requirement (code should work without it)
        //   but this situation would exacerbate a design flaw (see note #flaw_593)
        assert(aNode->mNumMeshes == 0 || aNode->mNumChildren == 0);

        // Create an Object containing the Meshes of this node
        {
            Object object;
            for (std::size_t meshIdx = 0; meshIdx != aNode->mNumMeshes; ++meshIdx)
            {
                unsigned int globalMeshIndex = aNode->mMeshes[meshIdx];
                {
                    auto insertionResult = aMeshMap.insert(globalMeshIndex);
                    // Ensure the mesh was not already encountered
                    // TODO: later on, we actually want to recover the notion of "Object" instance,
                    // that might be present on several Nodes
                    assert(insertionResult.second);
                }
                aiMesh* mesh = aScene->mMeshes[globalMeshIndex];
                assert(mesh->HasPositions() && mesh->HasFaces());
                assert(hasTrianglesOnly(mesh));

                const MeshPart_Naive& meshPart = object.mParts.emplace_back(handleMesh(mesh));
                if (meshIdx == 0)
                {
                    object.mAabb = meshPart.mAabb;
                }
                else
                {
                    object.mAabb.uniteAssign(meshPart.mAabb);
                }

                result.mVerticesCount += mesh->mNumVertices;
                result.mIndicesCount += mesh->mNumFaces * 3;

                std::cout << std::string(2 * aLevel, ' ')
                    << "- Mesh " << globalMeshIndex << " '" << mesh->mName.C_Str() << "'"
                    << " with " << mesh->mNumVertices << " vertices, " << mesh->mNumFaces << " triangles"
                    << ", material '" << aScene->mMaterials[mesh->mMaterialIndex]->GetName().C_Str()
                    << "'."
                    << "\n"
                    << std::string(2 * aLevel + 2, ' ') << "- Normals: " << (mesh->HasNormals() ? "yes" : "no")
                    << ", tangents & bitangents: " << (mesh->HasTangentsAndBitangents() ? "yes" : "no")
                    << "\n"
                    << std::string(2 * aLevel + 2, ' ') << "- " << mesh->GetNumColorChannels() << " color channel(s), "
                    << mesh->GetNumUVChannels() << " UV channel(s)."
                    << "\n" << std::string(2 * aLevel + 2, ' ') << "- "
                    << mesh->mNumBones << " bones."
                    << "\n"
                    << std::string(2 * aLevel + 2, ' ') << "- AABB " << meshPart.mAabb << "."
                    << "\n"
                    ;

                //aWriter.write(mesh);

                //aWriter.forward(meshAabb);
            }
            if (!object.mParts.empty())
            {
                aOutScene.mObjectsMap.emplace(thisIndex, std::move(object));
            }
        }


#if 0
        // Rig of the Node
        {
            // Node rig count (can only have one at the moment)
            unsigned int rigCount = nodeRig ? 1 : 0;
            aWriter.forward(rigCount);
            if(nodeRig)
            {
                result.mNodeRig = nodeRig;

                const Rig & rig = nodeRig->mRig;
// Run some more sanity checks on the Rig
#if not defined(NDEBUG)
                // Let's ensure that all bones, from the different meshes,
                // point to distinct Nodes in the hierarchy.
                const auto & indices = rig.mJoints.mIndices;
                std::vector<NodeTree<Rig::Pose>::Node::Index> sortedIndices(indices.size(), 0);
                std::partial_sort_copy(indices.begin(), indices.end(),
                                       sortedIndices.begin(), sortedIndices.end());
                
                // If this trips, it means several bones pointed to the same aiNode,
                // and this should never be the case because we take explicit measures to implement bone deduplication.
                // Note: Joint duplicates happen in the Assimp model because of this design:
                // The Bones are stored in the meshes.
                // So, if several meshes are influenced by the same bone in a logical skeleton,
                // each meach probably stores a bone to reference to the same node.
                assert(std::adjacent_find(sortedIndices.begin(), sortedIndices.end()) == sortedIndices.end()
                    && "Duplicate bones found in the rig");
#endif
                // Joint Tree
                const NodeTree<Rig::Pose> & jointTree = rig.mJointTree;
                // Writes the number of elements first
                aWriter.writeRaw(std::span{jointTree.mHierarchy});
                aWriter.forward(jointTree.mFirstRoot);
                // Does not write number of elements (will be the same)
                aWriter.forward(std::span{jointTree.mLocalPose});
                aWriter.forward(std::span{jointTree.mGlobalPose});
                aWriter.forward(jointTree.mNodeNames);

                // Joint Data
                // Writes the number of joints first
                aWriter.writeRaw(std::span{rig.mJoints.mIndices});
                aWriter.forward(std::span{rig.mJoints.mInverseBindMatrices});

                // Armature name
                aWriter.forward(rig.mArmatureName);
            }
        }

        // AABB of the Node
        aWriter.forward(result.mAabb); // this is the AABB of all direct parts, without children nodes. (i.e the `Object`).
#endif

        // TODO do we really want to compute the node's AABB? This is tricky, because there might be transformations
        // between nodes.
        // Yet the client will usually be interested in the top node AABB to define camera parameters...
        for(std::size_t childIdx = 0; childIdx != aNode->mNumChildren; ++childIdx)
        {
            NodeResult childResult = 
                recurseNodes(aNode->mChildren[childIdx], aScene, aOutScene, aMeshMap, thisIndex, aLevel + 1);

            //if(childIdx == 0 && aNode->mNumMeshes == 0) // The box was not primed yet
            //{
            //    result.mAabb = childResult.mAabb;
            //}
            //else
            //{
            //    result.mAabb.uniteAssign(childResult.mAabb);
            //}

            // Accumulate total into result
            result.mVerticesCount += childResult.mVerticesCount;
            result.mIndicesCount  += childResult.mIndicesCount;
            //if(childResult.mNodeRig)
            //{
            //    // NOTE Ad 2024/03/06: This is currently a **hard** requirement.
            //    //   #animation #singlerig Assimp data-model keeps "Animation" decoupled from nodes (listed under the global scene),
            //    //   and it does not really have a notion of Rig.
            //    //   Currently, we assume all animations are targeting the single rig we allow.
            //    assert(!result.mNodeRig);
            //    result.mNodeRig = childResult.mNodeRig;
            //}
        }

        // This is now the AABB including the children nodes.
        //aWriter.forward(result.mAabb);

        return result;
    }

    Handle<const VertexStream> prepareBuffers(const NodeResult& aCounts)
    {
        
    }

} // anonymous namespace

SceneTree loadModel(const std::filesystem::path& aModelFile, Context& aContext, float aGlobalScale)
{
    SceneTree result;

#if defined(VERBOSE_ASSIMP)
    // Comment out to get verbose output from the importer, to stdout.
    Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE, aiDefaultLogStream_STDOUT);
#endif

    // Create an instance of the Importer class
    Assimp::Importer importer;
    importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, aGlobalScale);
    // This is really extra verbose
    //importer.SetExtraVerbose(true); 

    // Have the importer read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll 
    // propably to request more postprocessing than we do in this example.
    const aiScene* scene = importer.ReadFile(
        aModelFile.string(),
        aiProcess_CalcTangentSpace       | 
        aiProcess_Triangulate            |
        aiProcess_JoinIdenticalVertices  |
        aiProcess_SortByPType            |
        // Ad: added flags below
        aiProcess_RemoveRedundantMaterials  |
        aiProcess_GenSmoothNormals          |
        aiProcess_ValidateDataStructure     |
        aiProcess_FindDegenerates           |
        // Note: Will remove "invalid" channels (e.g. all zero normals / tangents)
        // This will causes issues when the UV / Tangent are not valid: it removes them
        // which trips the `Processor` atm.
        aiProcess_FindInvalidData           |
        // Generate bouding boxes, see: https://stackoverflow.com/a/74331859/1027706
        aiProcess_GenBoundingBoxes          |
        // Populate the mNode member of aiBone, so we do not have to manually
        // match up on node names
        aiProcess_PopulateArmatureData      |
        // Limit the maximum number of bones affecting a single vertex
        // (default limit is 4)
        aiProcess_LimitBoneWeights          |
        // Allow to apply a scale factor via "AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY"
        (aGlobalScale != 1.0f ? aiProcess_GlobalScale : 0) |

        /* to allow final | */0
    );
    
    // If the import failed, report it
    if(!scene)
    {
        ADLOG(critical)(importer.GetErrorString());
        return result;
    }

    // Uncomment to get a list of all metadata keys and their type
    //const auto & md = *scene->mMetaData;
    //for(unsigned int i = 0; i != md.mNumProperties; ++i)
    //{
    //    int type = md.mValues[i].mType;
    //    SELOG(info)("Prop name: {} of type {}.", md.mKeys[i].C_Str(), type);

    //    //// To get the values, have to declare v of the correct type
    //    //int v = -1000;
    //    //bool getResult = md.Get(md.mKeys[i], v);
    //    //SELOG(warn)("Prop name [{}]:  {} -> {} of type {}.", getResult, md.mKeys[i].C_Str(), v, type);
    //}

    // Note: we have a problem with the FBX scaling, it seems the model vertices position are expressed in centimeters
    // The debug output of the importer state "Debug, T45804: UpdateImporterScale scale set: 0.01"
    // but "UnitScaleFactor" seems to be 1.
    float unitScaleFactor = 1.f;
    if(!scene->mMetaData->Get("UnitScaleFactor", unitScaleFactor))
    {
        ADLOG(info)("Could not read 'UnitScaleFactor' metadata.");
    }
    else if(unitScaleFactor != 1.)
    {
        ADLOG(warn)("Unit scale factor is {}.", unitScaleFactor);
    }

    // Now we can access the file's contents. 
    NodeResult nodeResult = countVerticesRecurse(scene->mRootNode, scene);
    ADLOG(info)("Loading file '{}', containing {} vertices an {} indices.",
        aModelFile, nodeResult.mVerticesCount, nodeResult.mIndicesCount);

    // Note: this could fail if meshes are reused (instancing)
    // or if some meshes are not present in any nodes (can we filter that out?)
    // (When this trips is a good time to handle instancing)
    assert(nodeResult == countVerticesDirect(scene->mRootNode, scene));

    MeshMap meshMap;
    recurseNodes(scene->mRootNode, scene, result, meshMap, Node::gInvalidIndex);

    return result;
}


} // namespce ad::scenic