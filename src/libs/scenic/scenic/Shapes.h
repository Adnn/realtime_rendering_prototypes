#pragma once


#include <math/Vector.h>

#include <vector>


namespace ad::scenic {

using Position = math::Vec<3, float>;
using Normal = math::Vec<3, float>;
using Index = std::uint32_t;

namespace quad {

    constexpr Position gVertices[4]{
        // Full-screen quad coordinates
        { -1.0f, -1.0f, 0.0f },
        {  1.0f, -1.0f, 0.0f },
        { -1.0f,  1.0f, 0.0f },
        {  1.0f,  1.0f, 0.0f },
    };

    constexpr Normal gNormals[4]{
        // Full-screen quad coordinates
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f },
    };

    // To validate when needed
    //constexpr Index gIndices[6] {
    //    0, 1, 2,
    //    2, 1, 3,
    //};

} // namespace quad

namespace icosahedron {

    constexpr float gPhi = 1.61803398875f;  // Golden ratio for icosahedron

    constexpr Position gVertices[12]{
    //    {-1.f,  gPhi,  0.f}.normalize(), {1.f,  gPhi,  0.f}, {-1.f, -gPhi,  0.f}, {1.f, -gPhi,  0.f},
    //    { 0.f, -1.f,  gPhi}, {0.f,  1.f,  gPhi}, { 0.f, -1.f, -gPhi}, {0.f,  1.f, -gPhi},
    //    { gPhi,  0.f, -1.f}, {gPhi,  0.f,  1.f}, {-gPhi,  0.f, -1.f}, {-gPhi, 0.f,  1.f}
    //};
    // Normalized coordinates:
        { -0.525731f,  0.850651f,  0.000000f },
        {  0.525731f,  0.850651f,  0.000000f },
        { -0.525731f, -0.850651f,  0.000000f },
        {  0.525731f, -0.850651f,  0.000000f },
        {  0.000000f, -0.525731f,  0.850651f },
        {  0.000000f,  0.525731f,  0.850651f },
        {  0.000000f, -0.525731f, -0.850651f },
        {  0.000000f,  0.525731f, -0.850651f },
        {  0.850651f,  0.000000f, -0.525731f },
        {  0.850651f,  0.000000f,  0.525731f },
        { -0.850651f,  0.000000f, -0.525731f },
        { -0.850651f,  0.000000f,  0.525731f }
    };

    constexpr Index gIndices[60] {
        0, 11, 5,   0,  5,  1,   0,  1,  7,   0, 7, 10,   0, 10, 11,
        1,  5, 9,   5, 11,  4,  11, 10,  2,  10, 7,  6,   7,  1,  8,
        3,  9, 4,   3,  4,  2,   3,  2,  6,   3, 6,  8,   3,  8,  9,
        4,  9, 5,   2,  4, 11,   6,  2, 10,   8, 6,  7,   9,  8,  1
    };

} // namespace icosahedron

namespace geodesic {

    struct Sphere
    {
        Sphere(unsigned int aSubdivisions);

        void subdivide();

        std::vector<Position> mVertices;
        std::vector<Index> mIndices;
    };
    
} // namespace geodesic

} // namespce ad::scenic
