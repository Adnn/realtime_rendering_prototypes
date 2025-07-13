#include "Shadow.h"

#include <graphics/CameraUtilities.h>

#include <math/Base.h>
#include <math/Box.h>
#include <math/LinearMatrix.h>

#include <renderer/BufferLoad.h>
#include <renderer/BufferIndexedBinding.h>

#include <scenic/Camera.h>
#include <scenic/environment/EnvironmentUtilities.h>


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


math::AffineMatrix<4, GLfloat> canonicalToLight(const renderer::PointLight_glsl & aPointLight)
{
    return math::AffineMatrix<4, GLfloat>{
        math::LinearMatrix<3, 3, GLfloat>::Identity(), // No rotation, align on canonical space
            -aPointLight.mPosition.as<math::Vec>()
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

    glBindBuffer(mCubeFacesViewBuffer.GLTarget_v, mCubeFacesViewBuffer); // For creation
    glObjectLabel(GL_BUFFER, mCubeFacesViewBuffer, -1, "CubeFacesViewProjection");
}


void Shadow::renderShadowMaps(const scenic::SceneTree & aSceneTree,
                              const renderer::LightsDataCommon & mLights,
                              FrameGraph & aGraph)
{
    renderer::LightViewProjection lightViewProjection;
    const math::Box<GLfloat> sceneAabb = getAabb(aSceneTree);

    //
    // Directional
    //
    {
        // We need to restore the previously bound viewprojection,
        // corresponding to the camera and assumed present by most of the code
        // This binding can be used for all directional lights
        graphics::ScopedBind boundViewProjection{
            mLightViewBuffer,
            graphics::BindingIndex{ 0 }};

        // TODO: render to all shadow maps layers at once via geometry shader instancing
        for (std::size_t directionalIdx = 0;
             directionalIdx != std::min(mLights.mDirectionalCount, renderer::gMaxShadowLights);
             ++directionalIdx)
        {
            const renderer::DirectionalLight_glsl & light = mLights.mDirectionalLights[directionalIdx];

            math::LinearMatrix<3, 3, GLfloat> worldToLightOrientation = alignMinusZ(light.mDirection);
            math::Matrix<4, 4, float> projection = computeLightProjection(worldToLightOrientation,
                                                                          sceneAabb);

            lightViewProjection.mLightViewProjections[lightViewProjection.mLightViewProjectionCount] =
                math::AffineMatrix<4, GLfloat>{worldToLightOrientation} * projection;
            ++lightViewProjection.mLightViewProjectionCount;

            graphics::loadSingle(mLightViewBuffer,
                                 scenic::GpuViewProjectionBlock{
                                    worldToLightOrientation,
                                    projection,
                                 },
                                 graphics::BufferHint::StreamDraw);

            glNamedFramebufferTextureLayer(aGraph.mShadowFramebuffer, GL_DEPTH_ATTACHMENT, aGraph.mShadowMap, 0, directionalIdx);
            assert(glCheckNamedFramebufferStatus(aGraph.mShadowFramebuffer, GL_DRAW_FRAMEBUFFER)
                   == GL_FRAMEBUFFER_COMPLETE);

            aGraph.renderDepth(aSceneTree, FrameGraph::DepthMapType::TwoD);
        }
    }

    //
    // Point
    //
    {
        // This binding can be used for all omni lights rendering to a cubemap
        // TODO: this buffer now contains constant data that could be loaded once at startup
        graphics::ScopedBind boundViewProjection{
            mCubeFacesViewBuffer,
            graphics::BindingIndex{ 14 }};

        static const math::Matrix<4, 4, float> gProjection = graphics::makeProjection(graphics::PerspectiveParameters{
            .mAspectRatio = 1,
            .mVerticalFov = math::Degree<float>{90.f},
            // TODO: address near/far depending on the scene AABB relative to camera
            .mNearZ = -FrameGraph::gShadowCubeNearDistance,
            .mFarZ = -FrameGraph::gShadowCubeFarDistance,
        });

        constexpr auto neg = math::trans3d::scale(1.f, -1.f, 1.f);
        std::array<math::Matrix<4, 4, GLfloat>, 6> viewProjections{
            scenic::gCubeCaptureViewsNegateY[0] * gProjection,
            scenic::gCubeCaptureViewsNegateY[1] * gProjection,
            scenic::gCubeCaptureViewsNegateY[2] * gProjection,
            scenic::gCubeCaptureViewsNegateY[3] * gProjection,
            scenic::gCubeCaptureViewsNegateY[4] * gProjection,
            scenic::gCubeCaptureViewsNegateY[5] * gProjection,
        };

        glNamedBufferData(mCubeFacesViewBuffer, sizeof(viewProjections), nullptr, GL_STREAM_DRAW);
        glNamedBufferSubData(mCubeFacesViewBuffer, 0, sizeof(viewProjections), viewProjections.data());

        // Since the texture is a cubemap, the framebuffer attachement is layered
        glNamedFramebufferTexture(aGraph.mShadowFramebuffer, GL_DEPTH_ATTACHMENT, aGraph.mOmniShadowMap, 0);
        assert(glCheckNamedFramebufferStatus(aGraph.mShadowFramebuffer, GL_DRAW_FRAMEBUFFER)
               == GL_FRAMEBUFFER_COMPLETE);

        aGraph.renderDepth(aSceneTree, FrameGraph::DepthMapType::CubeMap);

        // IMPORTANT: We do not populate the lightViewProjection UBO for omni lights.
        // The required fragToLight length is computed on the fly in the fragment shader.
    }

    //
    // Load lights view projections
    //
    graphics::loadSingle(aGraph.mLightViewProjectionUbo,
                         lightViewProjection,
                         graphics::BufferHint::StreamDraw);

}


} // namespace ad