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

    Index mParent      = gInvalidIndex;
    Index mFirstChild  = gInvalidIndex;
    Index mNextSibling = gInvalidIndex;
    Index mLastSibling = gInvalidIndex; // WARNING: this is an inconsistent internal value, not an invariant.
    unsigned int mLevel = 0;
};


template <class T_pose>
struct NodeTree
{
    /// @param aParent if set to gInvalidIndex, the call adds a root node.
    Node::Index addNode(Node::Index aParent, T_pose aLocalPose);

    std::vector<Node> mHierarchy;
    std::vector<T_pose> mLocalPose;
    std::vector<T_pose> mGlobalPose;

    // Alternatively, we could have an implicit unique root, and store a FirstChild here.
    Node::Index mFirstRoot = Node::gInvalidIndex;
};



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


} // namespce ad::scenic
