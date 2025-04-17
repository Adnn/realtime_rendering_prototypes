#pragma once


#include <math/Color.h>

#include <array>


namespace ad::scenic {


namespace sdr {

    //see: https://colorbrewer2.org/#type=qualitative&scheme=Set1&n=9
    // In sRGB color space
    constexpr std::array<math::sdr::Rgb, 9> gColorBrewerSet1_srgb {{
        {228,  26,  28},
        { 55, 126, 184},
        { 77, 175,  74},
        {152,  78, 163},
        {255, 127,   0},
        {255, 255,  51},
        {166,  86,  40},
        {247, 129, 191},
        {153, 153, 153},
    }};

    // Taken from: https://github.com/selfshadow/ltc_code/blob/31e5e96b54f98f33098f8503003119ba2231a1c6/fit/plot.h#L12-L47
    // The paper "Real-Time Polygonal-Light Shading with Linearly Transformed Cosines" seems to use it
    // to plot the linearly transformed cosines on the sphere.
    constexpr std::array<math::sdr::Rgb, 33> gLtcColorMap1D_srgb{ {
        { 59,  76,  192 },
        { 68,  90,  204 },
        { 77, 104,  215 },
        { 87, 117,  225 },
        { 98, 130,  234 },
        { 108, 142, 241 },
        { 119, 154, 247 },
        { 130, 165, 251 },
        { 141, 176, 254 },
        { 152, 185, 255 },
        { 163, 194, 255 },
        { 174, 201, 253 },
        { 184, 208, 249 },
        { 194, 213, 244 },
        { 204, 217, 238 },
        { 213, 219, 230 },
        { 221, 221, 221 },
        { 229, 216, 209 },
        { 236, 211, 197 },
        { 241, 204, 185 },
        { 245, 196, 173 },
        { 247, 187, 160 },
        { 247, 177, 148 },
        { 247, 166, 135 },
        { 244, 154, 123 },
        { 241, 141, 111 },
        { 236, 127,  99 },
        { 229, 112,  88 },
        { 222,  96,  77 },
        { 213,  80,  66 },
        { 203,  62,  56 },
        { 192,  40,  47 },
        { 180,   4,  38 },
    }};

} // namespace sdr


namespace hdr {

    constexpr math::hdr::Rgb_f gCameraFrustumColor = math::hdr::gYellow<float>;

    constexpr math::hdr::Rgb<GLfloat> gBrickAlbedo{ 0.262f, 0.095f, 0.061f };
    // See rtr 4th table 9.2 p323
    constexpr math::hdr::Rgb<GLfloat> gCopperAlbedo{ 0.955f, 0.638f, 0.538f };
    constexpr math::hdr::Rgb<GLfloat> gGoldAlbedo{ 1.000f, 0.782f ,0.344f };

    // In sRGB color space
    constexpr std::array<math::hdr::Rgb_f, 9> gColorBrewerSet1_srgb {{
        {0.894f, 0.102f, 0.110f},
        {0.216f, 0.494f, 0.722f},
        {0.302f, 0.686f, 0.290f},
        {0.596f, 0.306f, 0.639f},
        {1.000f, 0.498f, 0.000f},
        {1.000f, 1.000f, 0.200f},
        {0.651f, 0.337f, 0.157f},
        {0.969f, 0.506f, 0.749f},
        {0.600f, 0.600f, 0.600f},
    }};

    // Until we have C++26 for constexpr cmath, this is not constexpr
//    constexpr std::array<math::hdr::Rgb_f, 9> gColorBrewerSet1_linear {
//        []
//        {
//#if defined(_MSC_VER) && !defined(__clang__)
//            // I suspect a compiler bug with MSVC compiler, with message:
//            // > failure was caused by a read of an uninitialized symbol
//            // on MatrixBase default ctor calling setZero().
//            std::array<math::hdr::Rgb_f, 9> transformed{gColorBrewerSet1_srgb};
//#else
//            std::array<math::hdr::Rgb_f, 9> transformed;
//#endif
//            std::transform(gColorBrewerSet1_srgb.begin(), gColorBrewerSet1_srgb.end(),
//                           transformed.begin(),
//                           math::decode_sRGB<float>);
//            return transformed;
//        }()
//    };

    // Values computed offline from the above table, and copied back here.
    constexpr std::array<math::hdr::Rgb_f, 9> gColorBrewerSet1_linear {{
        {0.776f, 0.010f, 0.012f},
        {0.038f, 0.209f, 0.479f},
        {0.074f, 0.429f, 0.068f},
        {0.314f, 0.076f, 0.366f},
        {1.000f, 0.212f, 0.000f},
        {1.000f, 1.000f, 0.033f},
        {0.381f, 0.093f, 0.021f},
        {0.930f, 0.220f, 0.521f},
        {0.319f, 0.319f, 0.319f},
    }};

} // namespace hdr

} // namespace ad::scenic