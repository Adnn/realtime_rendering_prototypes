#pragma once


#include "shaders.h"

#include <scenic/Shapes.h>

#include <graphics/Timer.h>
#include <renderer/Drawing.h>
#include <renderer/VertexSpecification.h>

#include <math/Color.h>
#include <math/Vector.h>


namespace ad {


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
    Scene();

    void step(const graphics::Timer & aTimer);
    void render();

    graphics::VertexSpecification mVertexSpecification;
    graphics::IndexBufferObject mIndexBuffer;
    graphics::Program mProgram;
};


inline Scene::Scene() :
    mVertexSpecification{},
    mIndexBuffer{
        graphics::loadIndexBuffer(mVertexSpecification.mVertexArray,
                                  std::span{scenic::icosahedron::gIndices},
                                  graphics::BufferHint::StaticDraw)},
    mProgram{graphics::makeLinkedProgram({
              {GL_VERTEX_SHADER,   gVertexShader},
              {GL_FRAGMENT_SHADER, gFragmentShader},
    })}
{
    graphics::attachIndexBuffer(mIndexBuffer, mVertexSpecification.mVertexArray);

    graphics::appendToVertexSpecification(
        mVertexSpecification,
        gVertexDescription,
        std::span{scenic::icosahedron::gPositions},
        graphics::BufferHint::StaticDraw);

    graphics::appendToVertexSpecification(
        mVertexSpecification,
        gInstanceDescription,
        std::span{gInstances},
        graphics::BufferHint::StaticDraw,
        1);
}


inline void Scene::step(const graphics::Timer & /*aTimer*/)
{}


inline void Scene::render()
{
    glBindVertexArray(mVertexSpecification.mVertexArray);
    glUseProgram(mProgram);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDrawElementsInstanced(
        GL_TRIANGLES,
        static_cast<GLsizei>(std::size(scenic::icosahedron::gIndices)),
        graphics::MappedGL_v<std::remove_cvref_t<
            decltype(*scenic::icosahedron::gIndices)>>,
        0,
        static_cast<GLsizei>(std::size(gInstances)));
}


} // namespace ad
