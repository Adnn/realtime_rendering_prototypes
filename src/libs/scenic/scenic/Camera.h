#pragma once


#include <graphics/CameraUtilities.h>

#include <math/Angle.h>
#include <math/EulerAngles.h>
#include <math/Homogeneous.h>
#include <math/Spherical.h>
#include <math/Vector.h>

#include <variant>


namespace ad::scenic {


/// @brief Models a camera in the world, with a spatial-pose and a projection.
/// It can be used to generate eye transformation and projection transformation matrices.
class Camera
{
public:
    using OrthographicParameters = graphics::OrthographicParameters;
    using PerspectiveParameters = graphics::PerspectiveParameters;

    enum Projection : int
    {
        Orthographic = 0,
        Perspective = 1,
    };

    //math::Matrix<4, 4, GLfloat> assembleViewMatrix() const
    //{ return mWorldToCamera * mProjection; }

    void setPose(math::AffineMatrix<4, float> aParentToCamera)
    { mParentToCamera = aParentToCamera; }

    void setupOrthographicProjection(OrthographicParameters aParams);

    void setupPerspectiveProjection(PerspectiveParameters aParams);

    void setupProjection(OrthographicParameters aParams)
    { return setupOrthographicProjection(aParams); }

    void setupProjection(PerspectiveParameters aParams)
    { return setupPerspectiveProjection(aParams); }

    const math::AffineMatrix<4, float> & getParentToCamera() const
    { return mParentToCamera; }

    const math::Matrix<4, 4, float> & getProjection() const
    { return mProjection; }

    math::Matrix<4, 4, float> assembleViewProjection() const
    { return mParentToCamera * mProjection; }

    const std::variant<OrthographicParameters, PerspectiveParameters> & getProjectionParameters() const
    { return mProjectionParameters; }

    Projection identifyProjection() const
    { 
        return 
            std::holds_alternative<OrthographicParameters>(mProjectionParameters) ?
                Orthographic : Perspective; 
    }

    /// @brief Switch to the selected projection.
    /// @param aStablePlaneDistance The positive distance between the camera origin and a plane
    ///        whose image will stay the same in both projections.
    /// @return The projection that was active when the function was called.
    Projection switchProjection(Projection aSelected, float aStablePlaneDistance);

private:
    // TODO Ad 2024/05/28: #interpolation The camera pose should be expressed as a TRS to interpolate
    math::AffineMatrix<4, float> mParentToCamera = math::AffineMatrix<4, float>::Identity(); 
    math::Matrix<4, 4, float> mProjection = graphics::makeProjection(OrthographicParameters{
            .mAspectRatio = 1,
            .mViewHeight = 2,
            .mNearZ = +1,
            .mFarZ = -1,
        }); 

    std::variant<OrthographicParameters, PerspectiveParameters> mProjectionParameters;
};


/// @brief Helper method to set `aCamera` projection as orthographic with viewed height `aNewHeight`.
/// @param aCamera 
/// @param aNewHeight viewport height in world coordinates.
void changeOrthographicViewportHeight(Camera & aCamera, float aNewHeight);


void changeAspectRatio(Camera & aCamera, float aNewRatio);


std::pair<float/*near*/, float/*far*/> getNearFarPlanes(const Camera & aCamera);


/// @brief Layout compatible with shader's `ViewProjectionBlock`
struct GpuViewProjectionBlock
{
    GpuViewProjectionBlock(
        math::AffineMatrix<4, GLfloat> aWorldToCamera,
        math::Matrix<4, 4, GLfloat> aProjection 
    ) :
        mWorldToCamera{aWorldToCamera},
        mCameraToWorld{aWorldToCamera.inverse()},
        mProjection{aProjection},
        mViewingProjection{aWorldToCamera * aProjection},
        mCameraPosition_world{
            // Note: this is equivalent to {0, 0, 0, 1} * mCameraToWorld.
            math::homogeneous::makePosition(
                mCameraToWorld.getAffine().as<math::Position>())}
    {}

    GpuViewProjectionBlock(const Camera & aCamera) :
        GpuViewProjectionBlock{aCamera.getParentToCamera(), aCamera.getProjection()}
    {}

    math::AffineMatrix<4, GLfloat> mWorldToCamera; 
    math::AffineMatrix<4, GLfloat> mCameraToWorld; 
    math::Matrix<4, 4, GLfloat> mProjection; 
    math::Matrix<4, 4, GLfloat> mViewingProjection;
    math::Position<4, GLfloat> mCameraPosition_world;
};


//
// Poses
//

// TODO factorize, this is more general than purely camera movements.

/// @brief `Orbital` represent a position on a spherical orbit around an origin.
/// This can be used to represent camera poses, with the camera placed at the `Orbital` position,
/// and looking toward the orbit origin.
struct Orbital
{
    Orbital(float aRadius = 1.f,
            math::Radian<float> aPolar = math::Degree<float>{90.f},
            math::Radian<float> aAzimuthal = math::Radian<float>{0.f},
            math::Position<3, float> aPosition = {0.f, 0.f, 0.f}) :
        mSpherical{aRadius, aPolar, aAzimuthal}
    {}

    /// \warning aAzimuthal first, aPolar second (matches the usual convention for x and y)
    void incrementOrbit(math::Radian<float> aAzimuthal, math::Radian<float> aPolar);
    void incrementOrbitRadians(math::Vec<2, float> aIncrements)
    { incrementOrbit(math::Radian<float>{aIncrements.x()}, math::Radian<float>{aIncrements.y()}); }

    void pan(math::Vec<2, float> aPanning);

    float & radius();
    float radius() const;

    math::AffineMatrix<4, float> getParentToLocal() const;

    math::Position<3, float> mSphericalOrigin;
    math::Spherical<float> mSpherical;
};


/// \brief Position and orientation as Euler angles
struct SolidEulerPose
{
    math::AffineMatrix<4, float> getParentToLocal() const;

    // TODO Ad 2023/08/10: Implement conversions to math::Frame
    // Then use the Frame with math::trans3d::canonicaToFrame to implement getParentToLocal()
    /// @brief The position in the parent frame.
    math::Position<3, float> mPosition;
    /// @brief The orientation in the parent frame.
    math::EulerAngles<float> mOrientation;
};


//
// Controls over pose
//

//
//TODO Ad 2023/07/27: 
// This should be abstracted away from being used purely for rendering cameras

// TODO Ad 2024/02/16:
// OrbitalControl should be renamed and become a glfw-callback wrapper around some "OrbitalControl_raw".

/// @brief Controls an Orbital position with mouse movements (movements of an usual orbital camera).
struct OrbitalControl
{
    explicit OrbitalControl(Orbital aInitialPose) :
        mOrbital{aInitialPose}
    {}

    // Glfw compatible callbacks
    void callbackMouseButton(int button, int action, int mods, double xpos, double ypos);
    void callbackCursorPosition(double xpos, double ypos);
    void callbackScroll(double xoffset, double yoffset);
    void callbackKeyboard(int key, int scancode, int action, int mods)
    {}

    // Note: Initially, this class was storing a copy of the VFOV,
    // and thus could do panning directly in the cursor position callback.
    // Yet this copy violated DRY, and was only behaving well with perspective projection.
    // Note: As an alternative to taking the window size, the class could store a pointer to the appinterface
    // and query when needed.
    void update(float aViewHeightInWorld, int aWindowHeight);

    Orbital mOrbital;

private:
    enum class ControlMode
    {
        None,
        Orbit,
        Pan,
    };

    static constexpr math::Vec<2, float> gMouseControlFactor{1/700.f, 1/700.f};
    static constexpr float gScrollFactor = 0.05f;

    // The drag quantity in cursor unit (usually pixels)
    // This allow deferring the actual panning until update(), which can convert this quantity to world unit.
    math::Vec<2, float> mDragVector_cursor;
    ControlMode mControlMode{ControlMode::None};
    math::Position<2, float> mPreviousDragPosition{0.f, 0.f};
};


struct FirstPersonControl
{
    // Glfw compatible callbacks
    void callbackMouseButton(int button, int action, int mods, double xpos, double ypos);
    void callbackCursorPosition(double xpos, double ypos);
    void callbackScroll(double xoffset, double yoffset);
    void callbackKeyboard(int key, int scancode, int action, int mods);

    void update(float aDeltaTime);

    SolidEulerPose mPose;

private:
    enum class ControlMode
    {
        None,
        Aiming,
    };

    static constexpr math::Vec<2, float> gMouseControlFactor{1/1000.f, 1/1000.f};
    static constexpr float gSpeed = 4.f; // worldunit/s

    ControlMode mControlMode{ControlMode::None};
    math::Position<2, float> mPreviousCursorPosition{0.f, 0.f};

    bool mF{false}, mB{false}, mL{false}, mR{false};
};

} // namespace ad::scenic
