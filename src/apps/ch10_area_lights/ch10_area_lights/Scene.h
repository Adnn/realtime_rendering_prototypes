#pragma once


#include "CameraSystem.h"

#include <graphics/Timer.h>

#include <math/Color.h>
#include <math/Vector.h>

#include <renderer/UniformBuffer.h>
#include <renderer/VertexSpecification.h>
#include <renderer/Drawing.h>

#include <scenic/Shapes.h>


namespace ad {


namespace graphics {
    class AppInterface;
} // namespace graphics


constexpr graphics::AttributeDescriptionList gVertexDescription{
    {0, 3, /*offset*/0, graphics::MappedGL<GLfloat>::enumerator},
};


struct Instance
{
    math::hdr::Rgb_f mColor = math::hdr::gGreen<float>;
};

constexpr graphics::AttributeDescriptionList gInstanceDescription{
    {1, 3, offsetof(Instance, mColor), graphics::MappedGL<GLfloat>::enumerator},
};


static std::array<Instance, 1> gInstances{};


struct Scene
{
    Scene(graphics::AppInterface & aAppInterface);

    void step(
        const graphics::Timer & aTimer,
        math::Size<2, int> aWindowResolution);

    void render(math::Size<2, int> aRenderResolution);

    graphics::VertexSpecification mVertexSpecification;
    graphics::IndexBufferObject mIndexBuffer;
    graphics::UniformBufferObject mViewProjectionBuffer;
    graphics::Program mProgram;

    OrbitalCamera mOrbitalCamera;
};


} // namespace ad
