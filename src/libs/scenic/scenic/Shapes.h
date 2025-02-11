#pragma once


#include <math/Vector.h>


namespace ad::scenic {

namespace icosahedron {

    constexpr float gPhi = 1.61803398875;  // Golden ratio for icosahedron

    constexpr math::Vec<3, float> gPositions[12] {
        {-1.f,  gPhi,  0.f}, {1.f,  gPhi,  0.f}, {-1.f, -gPhi,  0.f}, {1.f, -gPhi,  0.f},
        { 0.f, -1.f,  gPhi}, {0.f,  1.f,  gPhi}, { 0.f, -1.f, -gPhi}, {0.f,  1.f, -gPhi},
        { gPhi,  0.f, -1.f}, {gPhi,  0.f,  1.f}, {-gPhi,  0.f, -1.f}, {-gPhi, 0.f,  1.f}
    };

    constexpr unsigned int gIndices[60] {
        0, 11, 5,   0,  5,  1,   0,  1,  7,   0, 7, 10,   0, 10, 11,
        1,  5, 9,   5, 11,  4,  11, 10,  2,  10, 7,  6,   7,  1,  8,
        3,  9, 4,   3,  4,  2,   3,  2,  6,   3, 6,  8,   3,  8,  9,
        4,  9, 5,   2,  4, 11,   6,  2, 10,   8, 6,  7,   9,  8,  1
    };

} // namespace icosahedron

} // namespce ad::scenic
