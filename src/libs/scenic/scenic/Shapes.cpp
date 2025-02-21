#include "Shapes.h"

namespace ad::scenic {

namespace geodesic {

    Sphere::Sphere(unsigned int aSubdivisions) :
        mVertices{ std::begin(icosahedron::gVertices), std::end(icosahedron::gVertices)},
        mIndices{ std::begin(icosahedron::gIndices), std::end(icosahedron::gIndices) }
    {
        std::vector<Index> subdividedIndices;
        Index nextIdx = (Index)mVertices.size();
        // Iterate each source triange, define by each group of 3 indices
        for(Index triangleStartIdx = 0;
            triangleStartIdx != mIndices.size();
            triangleStartIdx += 3)
        {
            // Iterate each edge of each triangle
            for(Index edge = 0; edge != 3; ++edge)
            {
                Position mid =
                    (mVertices[mIndices[triangleStartIdx + edge]] 
                        + mVertices[mIndices[triangleStartIdx + ((edge + 1) % 3)]])
                    / 2;
                mVertices.push_back(mid);
            }
            // The indices of the midpoints
            auto a = nextIdx++;
            auto b = nextIdx++;
            auto c = nextIdx++;
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