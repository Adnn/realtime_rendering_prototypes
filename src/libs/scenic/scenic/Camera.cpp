#include "Camera.h"

// TODO Address this dirty include, currently needed to get the GLFW input definitions
#include <GLFW/glfw3.h>


namespace ad::scenic {


////////
// Camera
////////


void Camera::setupOrthographicProjection(OrthographicParameters aParams)
{
    mProjection = graphics::makeProjection(aParams);
    mProjectionParameters = aParams;
}


void Camera::setupPerspectiveProjection(PerspectiveParameters aParams)
{
    mProjection = graphics::makeProjection(aParams);
    mProjectionParameters = aParams;
}


Camera::Projection Camera::switchProjection(Projection aSelected, float aStablePlaneDistance)
{
    Camera::Projection initialProjection = identifyProjection();
    if (aSelected != initialProjection)
    {
        switch(aSelected)
        {
            case Camera::Orthographic:
            {
                assert(initialProjection == Perspective);
                // Was perspective
                graphics::PerspectiveParameters perspective =
                    std::get<graphics::PerspectiveParameters>(mProjectionParameters);
                setupOrthographicProjection(toOrthographic(
                    perspective,
                    graphics::computePlaneHeight(aStablePlaneDistance, perspective.mVerticalFov)));
                break;
            }
            case Camera::Perspective:
            {
                assert(initialProjection == Orthographic);
                // Was orthograhic
                graphics::OrthographicParameters ortho = 
                    std::get<graphics::OrthographicParameters>(mProjectionParameters);
                setupPerspectiveProjection(toPerspective(
                    ortho,
                    graphics::computeVerticalFov(aStablePlaneDistance, ortho.mViewHeight)));
                break;
            }
        }
    }
    return initialProjection;
}


void changeOrthographicViewportHeight(Camera & aCamera, float aNewHeight)
{
    Camera::OrthographicParameters orthoParams = std::visit(
        [aNewHeight](auto aParams) -> Camera::OrthographicParameters
        {
            using T_param = std::decay_t<decltype(aParams)>;
            if constexpr(std::is_same_v<T_param, Camera::OrthographicParameters>)
            {
                aParams.mViewHeight = aNewHeight;
                return aParams;
            }
            else if constexpr(std::is_same_v<T_param, graphics::PerspectiveParameters>)
            {
                return graphics::toOrthographic(aParams, aNewHeight);
            }
        },
        aCamera.getProjectionParameters());

    aCamera.setupOrthographicProjection(orthoParams);
}


void changeAspectRatio(Camera & aCamera, float aNewRatio)
{
    std::visit(
        // aParams by copy, we modify the value before assigning it back
        [&aCamera, aNewRatio](auto aParams)
        {
            aParams.mAspectRatio = aNewRatio;
            aCamera.setupProjection(aParams);
        },
        aCamera.getProjectionParameters());
}


std::pair<float/*near*/, float/*far*/> getNearFarPlanes(const Camera & aCamera)
{
    return std::visit(
        [](auto aParams) -> std::pair<float, float>
        {
            return {aParams.mNearZ, aParams.mFarZ};
        },
        aCamera.getProjectionParameters());
}


////////
// Orbital
////////

void Orbital::incrementOrbit(math::Radian<float> aAzimuthal, math::Radian<float> aPolar)
{
    using Radian = math::Radian<float>;
    mSpherical.polar() += aPolar;
    mSpherical.azimuthal() += aAzimuthal;
    mSpherical.azimuthal() = reduce(mSpherical.azimuthal());
    mSpherical.polar() = std::max(Radian{0},
                                  std::min(Radian{math::pi<float>},
                                           mSpherical.polar()));
}


void Orbital::pan(math::Vec<2, float> aPanning)
{
    math::OrthonormalBase<3, float> tangent = mSpherical.computeTangentFrame().base;
    mSphericalOrigin -= aPanning.x() * tangent.u() - aPanning.y() * tangent.v();
}


float & Orbital::radius()
{
    return mSpherical.radius();
}


float Orbital::radius() const
{
    return mSpherical.radius();
}


math::AffineMatrix<4, float> Orbital::getParentToLocal() const
{
    const math::Frame<3, float> tangentFrame = mSpherical.computeTangentFrame();

    return math::trans3d::canonicalToFrame(math::Frame<3, float>{
        mSphericalOrigin + tangentFrame.origin.as<math::Vec>(),
        tangentFrame.base
    });
}


math::AffineMatrix<4, float> SolidEulerPose::getParentToLocal() const
{
    // Translate the world by negated camera position, then rotate by inverse camera orientation
    return math::trans3d::translate(-mPosition.as<math::Vec>()) 
        * math::toQuaternion(mOrientation).inverse().toRotationMatrix();
        // It is equivalent to:
        //* dummyToRotationMatrixInverse(mOrientation);
}


////////
// OrbitalControl
////////

void OrbitalControl::callbackCursorPosition(double xpos, double ypos)
{
    using Radian = math::Radian<float>;
    // top-left corner origin
    math::Position<2, float> cursorPosition{(float)xpos, (float)ypos};

    switch (mControlMode)
    {
        case ControlMode::Orbit:
        {
            auto angularIncrements = (cursorPosition - mPreviousDragPosition).cwMul(gMouseControlFactor);

            // The viewed object should turn in the direction of the mouse,
            // so the camera angles are changed in the opposite direction (hence the substractions).
            mOrbital.incrementOrbit(Radian{-angularIncrements.x()}, Radian{-angularIncrements.y()});
            break;
        }
        case ControlMode::Pan:
        {
            mDragVector_cursor = cursorPosition - mPreviousDragPosition;
            break;
        }
        default:
        {
            // pass
        }
    }

    mPreviousDragPosition = cursorPosition;
}


void OrbitalControl::update(float aViewHeightInWorld, int aWindowHeight)
{
    mOrbital.pan(mDragVector_cursor * aViewHeightInWorld / aWindowHeight);
    mDragVector_cursor = {0.f, 0.f};
}


void OrbitalControl::callbackMouseButton(int button, int action, int mods, double xpos, double ypos)
{
    if (action == GLFW_PRESS)
    {
        switch (button)
        {
            case GLFW_MOUSE_BUTTON_LEFT:
                mControlMode = ControlMode::Orbit;
                break;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                mControlMode = ControlMode::Pan;
                break;
        }
    }
    else if ((button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE)
             && action == GLFW_RELEASE)
    {
        mControlMode = ControlMode::None;
    }
}


void OrbitalControl::callbackScroll(double xoffset, double yoffset)
{
    float factor = (1 - (float)yoffset * gScrollFactor);
    mOrbital.radius() *= factor;
}


////////
// FirstPersonControl
////////

void FirstPersonControl::callbackMouseButton(int button, int action, int mods, double xpos, double ypos)
{
    if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT)
    {
        mControlMode = ControlMode::Aiming;
    }
    else if (action == GLFW_RELEASE && button == GLFW_MOUSE_BUTTON_LEFT)
    {
        mControlMode = ControlMode::None;
    }
}


void FirstPersonControl::callbackCursorPosition(double xpos, double ypos)
{
    // top-left corner origin (i.e. +Y is going down on the window)
    math::Position<2, float> cursorPosition{(float)xpos, (float)ypos};

    if(mControlMode == ControlMode::Aiming)
    {
        auto angularIncrements = (cursorPosition - mPreviousCursorPosition).cwMul(gMouseControlFactor);
        mPose.mOrientation.x.data() -= angularIncrements.y(); // mouse down -> +Y -> negative roation around X (to look down)
        mPose.mOrientation.y.data() -= angularIncrements.x(); // mouse right -> +X -> negative rotation around Y (to look right)

        // First clamp the vertical orientation (around X axis), then reduce
        static constexpr float quarterRevolution = math::Turn<float>{1.f/4.f}.as<math::Radian>().value();
        auto & x = mPose.mOrientation.x.data();
        x = std::clamp(x, -quarterRevolution, quarterRevolution);

        mPose.mOrientation = reduce(mPose.mOrientation);
    }

    mPreviousCursorPosition = cursorPosition;
}


void FirstPersonControl::callbackScroll(double xoffset, double yoffset)
{
    //TODO handle assigning only the callbacks that are present get rid of this empty
}


void FirstPersonControl::callbackKeyboard(int key, int scancode, int action, int mods)
{
    if(action != GLFW_PRESS && action != GLFW_RELEASE)
    {
        return;
    }

    switch(key)
    {
        case GLFW_KEY_I:
            mF = (action == GLFW_PRESS);
            break;
        case GLFW_KEY_K:
            mB = (action == GLFW_PRESS);
            break;
        case GLFW_KEY_J:
            mL = (action == GLFW_PRESS);
            break;
        case GLFW_KEY_L:
            mR = (action == GLFW_PRESS);
            break;
    }
}


/// @brief Rotation matrix applying rotations to align the parent frame to the EulerAngle basis.
/// i.e. a change of basis from local to parent.
math::LinearMatrix<3, 3, float> dummyToRotationMatrix(math::EulerAngles<float> aEuler)
{
    return math::trans3d::rotateX(aEuler.x)
        * math::trans3d::rotateY(aEuler.y)
        * math::trans3d::rotateZ(aEuler.z);
}


/// @brief A Change of basis from parent to local.
/// Or as an active transformation: rotate the EulerAngle basis to align it to its parent frame.
math::LinearMatrix<3, 3, float> dummyToRotationMatrixInverse(math::EulerAngles<float> aEuler)
{
    // Note in linear algebra (A * B)^-1 = B^-1 * A^-1,
    // and the inverse of a rotation matrix is the rotation by negated angle.
    return math::trans3d::rotateZ(-aEuler.z)
        * math::trans3d::rotateY(-aEuler.y)
        * math::trans3d::rotateX(-aEuler.x);
}


void FirstPersonControl::update(float aDeltaTime)
{
    float movement = aDeltaTime * gSpeed;
    // This is the rotation matrix in parent frame:
    // its rows are the base vectors of the camera frame, expressed in canonical coordinates.
    math::LinearMatrix<3, 3, float> rotation = toQuaternion(mPose.mOrientation).toRotationMatrix();
    // It is equivalent to:
    //math::LinearMatrix<3, 3, float> rotation = dummyToRotationMatrix(mOrientation);
    math::Vec<3, float> backward{rotation[2]}; // camera looks in -Z, so +Z is the backward vector
    math::Vec<3, float> right{rotation[0]};
    if(mF) mPose.mPosition -= backward * movement;
    if(mB) mPose.mPosition += backward * movement;
    if(mL) mPose.mPosition -= right * movement;
    if(mR) mPose.mPosition += right * movement;
}


} // namespace ad::scenic