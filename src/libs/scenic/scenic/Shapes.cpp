#include "Shapes.h"

#include <unordered_map>

#include <cassert>


namespace ad::scenic {

namespace geodesic {

    Sphere::Sphere(unsigned int aSubdivisions) :
        mVertices{ std::begin(icosahedron::gVertices), std::end(icosahedron::gVertices)},
        mIndices{ std::begin(icosahedron::gIndices), std::end(icosahedron::gIndices) }
    {
        for (; aSubdivisions != 0; --aSubdivisions)
        {
            subdivide();
        }
    }

    // Note: Avoid duplication of the created midpoints betwen the two shared edges.
    // TODO: there are optimizations to make: 
    //     * the size of both vectors is determined by subdivision count
    //     * it should be possible to write the indices list only once, at the lowest level
    //       (e.g. recursion, and write at last level only).
    void Sphere::subdivide()
    {
        std::unordered_map<std::uint64_t, Index> midPoints;

        auto bisect = [this](Index aLeft, Index aRight) -> Position
            {
                return (mVertices[aLeft] + mVertices[aRight]).normalize();
            };

        auto getMidpointIndex = [&midPoints, &bisect, this](Index aLeft, Index aRight) -> Index
            {
                std::pair<std::uint64_t, std::uint64_t> minmax = std::minmax(aLeft, aRight);
                std::uint64_t key = minmax.second << 32 | minmax.first;
                auto found = midPoints.find(key);
                if (found == midPoints.end())
                {
                    Index inserted = (Index)mVertices.size();
                    mVertices.push_back(bisect(aLeft, aRight));
                    midPoints.emplace(key, inserted);
                    return inserted;
                }
                else
                {
                    return found->second;
                }
            };

        std::vector<Index> subdividedIndices;
        // Iterate each source triangle, define by each group of 3 indices
        for (Index triangleStartIdx = 0;
            triangleStartIdx != mIndices.size();
            triangleStartIdx += 3)
        {
            Index midpoints[3];
            // Iterate each edge of each triangle
            for (Index edge = 0; edge != 3; ++edge)
            {
                midpoints[edge] = getMidpointIndex(
                    mIndices[triangleStartIdx + edge],
                    mIndices[triangleStartIdx + ((edge + 1) % 3)]);
            }
            // The indices of the midpoints
            auto a = midpoints[0];
            auto b = midpoints[1];
            auto c = midpoints[2];
            // The indices of the original triangle
            auto X = mIndices[triangleStartIdx];
            auto Y = mIndices[triangleStartIdx + 1];
            auto Z = mIndices[triangleStartIdx + 2];
            subdividedIndices.insert(
                subdividedIndices.end(),
                {
                    X, a, c,
                    c, a, b,
                    b, a, Y,
                    b, Z, c,
                }
            );
        }
        mIndices = std::move(subdividedIndices);
    }
    
} // namespace geodesic

} // namespce ad::scenic