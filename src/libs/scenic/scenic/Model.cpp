#include "Model.h"


namespace ad::scenic {

namespace {

    template <class T_value>
    void mergeMap(std::unordered_map<Node::Index, T_value> & aDestination,
                   std::unordered_map<Node::Index, T_value> & aMovedFrom,
                   Node::Index aKeyOffset)
    {
        for (auto & [key, value] : aMovedFrom)
        {
            static_assert(std::movable<T_value>);
            aDestination.emplace(
                key + aKeyOffset,
                std::move(value));
        }
    }

} // unnamed namespace

SceneTree & mergeScenes(SceneTree & aBaseTree,
                        SceneTree & aMerged,
                        Node::Index aParent)
{
    
    Node::Index appliedOffset = aBaseTree.mTree.insert(aMerged.mTree, aParent);
    mergeMap(aBaseTree.mObjectsMap, aMerged.mObjectsMap, appliedOffset);

    aMerged.mTree = {};
    aMerged.mObjectsMap.clear();

    return aBaseTree;
}


} // namespce ad::scenic