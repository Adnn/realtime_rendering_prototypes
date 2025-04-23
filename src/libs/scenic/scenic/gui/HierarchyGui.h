#pragma once


#include "../Hierarchy.h"
#include "../Pose.h"

#include <math/EulerAngles.h>

#include <imgui.h>

#include <optional>
#include <string>


namespace ad::scenic {

struct TreeInteractionState
{
    // allow a single selection (we could use a mask system to allow multiple selection)
    Node::Index mSelected = Node::gInvalidIndex;
};


/// @brief Present a NodeTree as a DearImgui tree
/// @return The index of the selected Node, -1 if none.
template <class T_pose>
void presentNodeTree(const NodeTree<T_pose> & aTree,
                     Node::Index aNode,
                     TreeInteractionState & aState)
{
    static const ImGuiTreeNodeFlags gBaseFlags = 
        ImGuiTreeNodeFlags_OpenOnArrow 
        | ImGuiTreeNodeFlags_OpenOnDoubleClick 
        | ImGuiTreeNodeFlags_SpanAvailWidth
        ;

    ImGuiTreeNodeFlags flags = 
        gBaseFlags
        | (aTree.hasChild(aNode) ? 0 : ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet)
        ;

    if (aState.mSelected == aNode)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    std::string storage;
    const std::string & name = aTree.getSafeName(aNode, storage);
    bool opened = ImGui::TreeNodeEx(&aTree.mHierarchy[aNode], flags, "%s", name.c_str());

    ImGui::PushID((int)aNode);

    if(// select on first click
       ImGui::IsItemClicked(0) 
       // avoid selecting the item when clicking on the arrow to toggle its open state
       && !ImGui::IsItemToggledOpen())
    {
        aState.mSelected = aNode;
    }

    if(opened)
    {
        for(Node::Index child = aTree.mHierarchy[aNode].mFirstChild;
            child != Node::gInvalidIndex;
            child = aTree.mHierarchy[child].mNextSibling)
        {
            presentNodeTree(aTree, child, aState);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}


/// @brief Present the GUI to control a Pose
/// @return `true` if the pose was modified
std::optional<Pose> presentPose(const Pose & aPose);


} // namespce ad::scenic
