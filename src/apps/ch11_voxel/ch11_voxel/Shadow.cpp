#include "Shadow.h"

#include <graphics/CameraUtilities.h>

#include <math/Base.h>
#include <math/Box.h>
#include <math/LinearMatrix.h>

#include <renderer/BufferLoad.h>
#include <renderer/BufferIndexedBinding.h>

#include <scenic/Camera.h>

namespace ad {


/// @brief Construct a rotation matrix transforming from canonical space to camera space.
/// @param aGazeDirection The camera view direction, expressed in canonical space.
///
/// The rotation matrix transforms coordinates from the basis in which
/// `aGazeDirection`  is defined, to a basis whose -Z is `aGazeDirection`.
/// (Alternatively, this matrix actively rotates aGazeDirection onto -Z.)
math::LinearMatrix<3, 3, GLfloat> alignMinusZ(math::Vec<3, GLfloat> aGazeDirection)
{
    
    math::Vec<3, GLfloat> up{0.f, 1.f, 0.f};
    if(std::abs(aGazeDirection.dot(up)) > 0.9f)
    {
        up = {0.f, 0.f, -1.f};
    }
    auto cameraBase = 
        math::OrthonormalBase<3, GLfloat>::MakeFromWUp(-aGazeDirection, up);
    
    const auto & u = cameraBase.u();
    const auto & v = cameraBase.v();
    const auto & w = cameraBase.w();

    // TODO Ad 2024/08/29: This should be a math function
    return {
        u.x(),          v.x(),          w.x(),
        u.y(),          v.y(),          w.y(),
        u.z(),          v.z(),          w.z(),
    };
}


math::Matrix<4, 4, float> computeLightProjection(math::LinearMatrix<3, 3, GLfloat> aOrientationWorldToLight,
                                                 math::Box<GLfloat> aSceneAabb)
{
    const math::Box<GLfloat> sceneAabb_light = aSceneAabb * aOrientationWorldToLight;
    const math::Rectangle<GLfloat> sceneAabbSide_light = sceneAabb_light.frontRectangle();

    auto far = sceneAabb_light.zMin();
    auto depth = sceneAabb_light.depth();

    math::Position<3, GLfloat> frustumBoxPosition{sceneAabbSide_light.mPosition, far};
    math::Size<3, GLfloat> frustumBoxDimension{sceneAabbSide_light.mDimension, depth};
    return graphics::makeOrthographicProjection({frustumBoxPosition, frustumBoxDimension});
}


Shadow::Shadow()
{
    glBindBuffer(mLightViewBuffer.GLTarget_v, mLightViewBuffer); // For creation
    glObjectLabel(GL_BUFFER, mLightViewBuffer, -1, "LightViewProjection");
}


void Shadow::renderShadowMaps(const scenic::SceneTree & aSceneTree,
                              const renderer::LightsDataCommon & mLights,
                              FrameGraph & aGraph)
{
    // TODO: extend to handle point lights and multiple lights
    assert(mLights.mDirectionalCount == 1 && mLights.mPointCount == 0);

    const renderer::DirectionalLight_glsl & light = mLights.mDirectionalLights[0];

    math::LinearMatrix<3, 3, GLfloat> worldToLightOrientation = alignMinusZ(light.mDirection);
    math::Matrix<4, 4, float> projection = computeLightProjection(worldToLightOrientation,
                                                                  getAabb(aSceneTree));

    graphics::loadSingle(mLightViewBuffer,
                         scenic::GpuViewProjectionBlock{
                            worldToLightOrientation,
                            projection,
                         },
                         graphics::BufferHint::StreamDraw);

    // We need to restore the previously bound viewprojection,
    // corresponding to the camera and assumed present by most of the code
    graphics::ScopedBind boundViewProjection{
        mLightViewBuffer,
        graphics::BindingIndex{ 0 }};

    aGraph.renderDepth(aSceneTree);

    renderer::LightViewProjection lightViewProjection
    {
        .mLightViewProjectionCount = 1,
    };
    lightViewProjection.mLightViewProjections[0] =
        math::AffineMatrix<4, GLfloat>{worldToLightOrientation} * projection;

    graphics::loadSingle(aGraph.mLightViewProjectionUbo,
                         lightViewProjection,
                         graphics::BufferHint::StreamDraw);

}


} // namespace ad