#pragma once


#include <math/Vector.h>

// TODO: should split this header into a separate helpers, to avoid including string
#include <string>
#include <vector>

#include <cassert>


namespace ad::scenic {


// TODO: is there a rationale to store this as a AoS instead of splitting it into a SoA?
/// @brief Group data of a logical "SceneNode" regarding its position in the hierarchy.
struct Node
{
    using Index = std::size_t;
    static constexpr Index gInvalidIndex = std::numeric_limits<Index>::max();

    bool operator==(const Node &) const = default;

    Index mParent      = gInvalidIndex; // Invalid index parent means root node
    Index mFirstChild  = gInvalidIndex;
    Index mNextSibling = gInvalidIndex;
    // WARNING: this is an inconsistent internal value, not an invariant.
    // We maintain it only for the first children
    Index mLastSibling = gInvalidIndex;
    unsigned int mLevel = 0;
};



template <class T_pose>
struct NodeTree
{
    std::size_t size() const;

    bool hasChild(Node::Index aNode) const;
    bool hasParent(Node::Index aNode) const;
    bool hasName(Node::Index aNode) const;

    /// @param aParent if set to gInvalidIndex, the call adds a root node.
    Node::Index addNode(Node::Index aParent, T_pose aLocalPose);

    void setLocalPose(Node::Index aNode, T_pose aLocalPose);
    void recurseGlobalPose(Node::Index aNode, const T_pose & aParentGlobalPose);

    /// @brief Return the name of the node if available, or a generated name between angle brackets.
    /// @return A reference to the string that contains the name, which **might not** be provided aClientStorage.
    [[nodiscard]] const std::string & getSafeName(Node::Index aNode,
                                                  std::string & aClientStorage) const;

    /// @brief Insert provided subtree into this NodeTree.
    /// @param aSubtree 
    /// @param aInsertionParent The parent for the inserted subtree.
    ///        The subtree is root if the value is gInvalidIndex.
    /// @return The offset applied to indices in the subtree hierarchy.
    Node::Index insert(const NodeTree & aSubtree,
                       Node::Index aInsertionParent = Node::gInvalidIndex);

    bool operator==(const NodeTree &) const = default;

    std::vector<Node> mHierarchy;
    std::vector<T_pose> mLocalPose;
    std::vector<T_pose> mGlobalPose;

    // Alternatively, we could have an implicit unique root, and store a FirstChild here.
    Node::Index mFirstRoot = Node::gInvalidIndex;
};


template <class T_pose>
NodeTree<T_pose> makeOneRootTree(T_pose aRootPose = {})
{
    return NodeTree<T_pose>{
        .mHierarchy{
            Node{
                .mLastSibling = 0,
            },
        },
        .mLocalPose{aRootPose},
        .mGlobalPose{aRootPose},
        .mFirstRoot = 0,
    };
}


template <class T_pose>
std::size_t NodeTree<T_pose>::size() const
{
    assert(mHierarchy.size() == mLocalPose.size()
           && mHierarchy.size() == mGlobalPose.size());
    return mHierarchy.size();
}


template <class T_pose>
bool NodeTree<T_pose>::hasChild(Node::Index aNode) const
{
    return mHierarchy[aNode].mFirstChild != Node::gInvalidIndex;
}


template <class T_pose>
bool NodeTree<T_pose>::hasParent(Node::Index aNode) const
{
    return mHierarchy[aNode].mParent != Node::gInvalidIndex;
}


template <class T_pose>
bool NodeTree<T_pose>::hasName(Node::Index aNode) const
{
    // TODO: implement node names
    return false;
}


template <class T_pose>
[[nodiscard]] const std::string & NodeTree<T_pose>::getSafeName(Node::Index aNode, std::string & aClientStorage) const
{
    if (hasName(aNode))
    {
#if !defined(NDEBUG)
        // Give a consistent message if the client wrongfully use the storage as result.
        aClientStorage = "<nulled>";
#endif
        // Should return the reference to the internally stored string, without copy to client storage
        /*return aTree.mNodeNames[aNode]*/
        throw std::logic_error{"not implemented"};
    }
    else
    {
        aClientStorage = "<node_" + std::to_string(aNode) + ">";
        return aClientStorage;
    }
}

template <class T_pose>
Node::Index NodeTree<T_pose>::addNode(Node::Index aParent, T_pose aLocalPose)
{
    Node::Index thisIndex = mHierarchy.size();
    mHierarchy.push_back(Node{
        .mParent = aParent,
    });

    mLocalPose.push_back(std::move(aLocalPose));

    Node & node = mHierarchy[thisIndex];

    if(aParent != Node::gInvalidIndex) // This node has a parent
    {
        // The parent index must be an index in the exisiting hierarchy, before this node.
        assert(aParent < (mHierarchy.size() - 1));

        Node & parent = mHierarchy[aParent];
        node.mLevel = parent.mLevel + 1;
        mGlobalPose.push_back(composeLeftToRight(mLocalPose[thisIndex], mGlobalPose[aParent]));

        if (parent.mFirstChild == Node::gInvalidIndex) // The parent has no child
        {
            parent.mFirstChild = thisIndex;
            node.mLastSibling = thisIndex;
        }
        else // The parent has children
        {
            Node & firstSibling = mHierarchy[parent.mFirstChild];
            Node::Index currentLastIdx = firstSibling.mLastSibling;
            // If the first sibling does not have its last sibling value set,
            // find the last sibling by traversing the siblings.
            if(currentLastIdx == Node::gInvalidIndex)
            {
                // Note: this situation is warning worthy, it should not show up
                assert(false);
                for(currentLastIdx = parent.mFirstChild;
                    mHierarchy[currentLastIdx].mNextSibling != Node::gInvalidIndex;
                    currentLastIdx = mHierarchy[currentLastIdx].mNextSibling)    
                {}
            }
            Node& lastSibling = mHierarchy[currentLastIdx];
            lastSibling.mNextSibling = thisIndex;
            firstSibling.mLastSibling = thisIndex;
        }
    }
    else // This is a root node
    {
        mGlobalPose.push_back(mLocalPose[thisIndex]);

        if (mFirstRoot != Node::gInvalidIndex) // This is not the first root node
        {
            assert(mHierarchy[mFirstRoot].mLastSibling != Node::gInvalidIndex);
            Node::Index currentLastIdx = mHierarchy[mFirstRoot].mLastSibling;

            mHierarchy[currentLastIdx].mNextSibling = thisIndex;
        }
        else // This is the first root node
        {
            mFirstRoot = thisIndex;
        }
        mHierarchy[mFirstRoot].mLastSibling = thisIndex;
    }

    return thisIndex;
}


template <class T_pose>
void NodeTree<T_pose>::setLocalPose(Node::Index aNode, T_pose aLocalPose)
{
    mLocalPose[aNode] = std::move(aLocalPose);
    recurseGlobalPose(aNode,
                      hasParent(aNode) ? mGlobalPose[mHierarchy[aNode].mParent]
                                         : T_pose{});
}


template <class T_pose>
void NodeTree<T_pose>::recurseGlobalPose(Node::Index aNode,
                                         const T_pose & aParentGlobalPose)
{
    mGlobalPose[aNode] = composeLeftToRight(mLocalPose[aNode], aParentGlobalPose);

    for(Node::Index childIdx = mHierarchy[aNode].mFirstChild;
        childIdx != Node::gInvalidIndex;
        childIdx = mHierarchy[childIdx].mNextSibling)    
    {
        recurseGlobalPose(childIdx, mGlobalPose[aNode]);
    }
}


// TODO: ideally would not be exposed to clients, or moved to a generic header
namespace utils {

template <class T_element>
std::vector<T_element> & append(std::vector<T_element> & aReceiver,
                                const std::vector<T_element> aAppended)
{
    aReceiver.reserve(aReceiver.size() + aAppended.size());
    aReceiver.insert(aReceiver.end(), aAppended.begin(), aAppended.end());
    return aReceiver;
}


inline void shiftNode(Node & aNode, Node::Index aOffset, unsigned int aLevelOffset)
{
#define SHIFT(member) if(aNode.##member != Node::gInvalidIndex) aNode.##member += aOffset

    SHIFT(mParent);
    SHIFT(mFirstChild);
    SHIFT(mNextSibling);
    SHIFT(mLastSibling);
    aNode.mLevel += aLevelOffset;

#undef SHIFT
}

template <class T_pose>
void appendSiblings(Node::Index & aFirstSibling, 
                    Node::Index aAppendedSibling,
                    NodeTree<T_pose> & aTree)
{
    if (aFirstSibling == Node::gInvalidIndex)
    {
        aFirstSibling = aAppendedSibling;
    }
    else
    {
        auto & lastSibling = aTree.mHierarchy[aFirstSibling].mLastSibling;
        assert(aTree.mHierarchy[lastSibling].mNextSibling == Node::gInvalidIndex);
        aTree.mHierarchy[lastSibling].mNextSibling = aAppendedSibling;
        lastSibling = aTree.mHierarchy[aAppendedSibling].mLastSibling;
    }
}


}; // namespace utils


template <class T_pose>
Node::Index NodeTree<T_pose>::insert(const NodeTree & aSubtree,
                                     Node::Index aInsertionParent)
{
    auto initialSize = size();
    // If this NodeTree is empty, copy the subtree over
    if (mFirstRoot == Node::gInvalidIndex)
    {
        assert(initialSize == 0);
        *this = aSubtree;
        return 0;
    }

    unsigned int insertionLevel = 0;
    if (aInsertionParent != Node::gInvalidIndex)
    {
        // Sanity check: insertion must be under a Node available in this
        assert(aInsertionParent < initialSize);
        insertionLevel = mHierarchy[aInsertionParent].mLevel + 1;
    }

    // Append the inserted data after this data
    utils::append(mHierarchy, aSubtree.mHierarchy);
    // Re-index inserted nodes
    for (Node::Index idx = initialSize; idx != mHierarchy.size(); ++idx)
    {
        utils::shiftNode(mHierarchy[idx], initialSize, insertionLevel);
    }
    Node::Index shiftedSubtreeRoot = aSubtree.mFirstRoot + initialSize;

    utils::append(mLocalPose, aSubtree.mLocalPose);

    if (aInsertionParent != Node::gInvalidIndex)
    {
        // Resize global pose container to receive values during the recursive descend
        mGlobalPose.resize(initialSize + aSubtree.size());
        recurseGlobalPose(shiftedSubtreeRoot, mGlobalPose[aInsertionParent]);

        utils::appendSiblings(mHierarchy[aInsertionParent].mFirstChild,
                              shiftedSubtreeRoot,
                              *this);
    }
    else // Insert the subtree as a root node
    {
        utils::append(mGlobalPose, aSubtree.mGlobalPose);
        utils::appendSiblings(mFirstRoot, shiftedSubtreeRoot, *this);
    }

    return initialSize;
}


} // namespce ad::scenic
