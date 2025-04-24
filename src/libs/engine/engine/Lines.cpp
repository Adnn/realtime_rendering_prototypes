#include "Lines.h"

#include <math/Angle.h>


namespace ad::renderer {


std::vector<math::Position<3, GLfloat>> makeRoundSegment(unsigned int aCircleResolution)
{
    std::vector<math::Position<3, GLfloat>> result = {
        {0.0f, -0.5f, 0.0f},
        {0.0f, -0.5f, 1.0f},
        {0.0f,  0.5f, 1.0f},
        {0.0f, -0.5f, 0.0f},
        {0.0f,  0.5f, 1.0f},
        {0.0f,  0.5f, 0.0f},
    };

    const math::Radian<GLfloat> pi{ math::pi<GLfloat> };
    math::Radian<GLfloat> step{ 2 * pi / aCircleResolution };

    auto pushCircle = [&](GLfloat aSide)
        {
            math::Position<3, GLfloat> center{0.f, 0.f, aSide};
            for (unsigned int i = 0; i != aCircleResolution; ++i)
            {
                result.push_back(center);
                result.push_back({0.5f * cos(i * step),       0.5f * sin(i * step),       aSide});
                result.push_back({0.5f * cos((i + 1) * step), 0.5f * sin((i + 1) * step), aSide});
            }
        };

    pushCircle(0.f);
    pushCircle(1.f);

    return result;
}

} // namespace ad::renderer
