#pragma once


#include <math/Vector.h>

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

    /// @param aParent if set to gInvalidIndex, the call adds a root node.
    Node::Index addNode(Node::Index aParent, T_pose aLocalPose);

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
std::size_t NodeTree<T_pose>::size() const
{
    assert(mHierarchy.size() == mLocalPose.size()
           && mHierarchy.size() == mGlobalPose.size());
    return mHierarchy.size();
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

    utils::append(mLocalPose, aSubtree.mLocalPose);

    if (aInsertionParent != Node::gInvalidIndex)
    {
        // TODO #scenegraph: Recalculate global poses
        assert(false);
    }
    else // Insert the subtree as a root node
    {
        utils::append(mGlobalPose, aSubtree.mGlobalPose);

        auto & lastRoot = mHierarchy[mFirstRoot].mLastSibling;
        assert(mHierarchy[lastRoot].mNextSibling == Node::gInvalidIndex);
        mHierarchy[lastRoot].mNextSibling = aSubtree.mFirstRoot + initialSize;
        lastRoot = aSubtree.mHierarchy[aSubtree.mFirstRoot].mLastSibling + initialSize;
    }

    return initialSize;
}


} // namespce ad::scenic
