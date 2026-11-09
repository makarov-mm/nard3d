#include "Camera.h"

#include <algorithm>
#include <cmath>

using namespace math;

namespace {

constexpr float MinPitch = 0.12f, MaxPitch = 1.5703f;
constexpr float OverviewPitch = 0.9f;
constexpr float FieldOfView = 0.6f;
constexpr float MinAspect = 1.5f; // of the space for the board, below which the view widens
constexpr float Ease = 7.0f;
constexpr Vec3 Middle{0.03f, 0, 0}; // of the board with its tray

// The angle nearest to `current` that points the same way as `angle`.
float Unwrap(float angle, float current)
{
    while (angle - current > Pi)
        angle -= 2 * Pi;
    while (angle - current < -Pi)
        angle += 2 * Pi;
    return angle;
}

} // namespace

BoardCamera::BoardCamera()
{
    ApplyMode();
    m_pose = m_goal;
}

void BoardCamera::SetMode(CameraMode mode)
{
    m_mode = mode;
    ApplyMode();
}

void BoardCamera::Reset()
{
    ApplyMode();
}

void BoardCamera::ApplyMode()
{
    m_goal.yaw = Unwrap(0, m_goal.yaw);
    if (m_mode == CameraMode::Top) {
        // Nearly straight down, tilted a little toward White so that the lamp is not mirrored in the whole board.
        m_goal.target = Middle;
        m_goal.pitch = 1.3f;
        m_goal.distance = 1.0f;
    }
    else {
        m_goal.target = Middle + Vec3{0, 0, 0.035f};
        m_goal.pitch = OverviewPitch;
        m_goal.distance = 0.92f;
    }
}

void BoardCamera::Rotate(float yawRadians, float pitchRadians)
{
    m_goal.yaw += yawRadians;
    m_goal.pitch = std::clamp(m_goal.pitch + pitchRadians, MinPitch, MaxPitch);
    if (m_mode == CameraMode::Top)
        m_mode = CameraMode::Overview;
}

void BoardCamera::Zoom(float steps)
{
    m_goal.distance = std::clamp(m_goal.distance * std::pow(0.88f, steps), 0.32f, 2.2f);
}

void BoardCamera::Update(float seconds)
{
    const float ease = 1 - std::exp(-seconds * Ease);
    m_pose.target = Lerp(m_pose.target, m_goal.target, ease);
    m_pose.yaw += (m_goal.yaw - m_pose.yaw) * ease;
    m_pose.pitch += (m_goal.pitch - m_pose.pitch) * ease;
    // The distance eases by ratio, so that zooming feels the same near and far.
    m_pose.distance *= std::pow(m_goal.distance / m_pose.distance, ease);
    m_pose.shift += (m_goal.shift - m_pose.shift) * ease;
    m_pose.space += (m_goal.space - m_pose.space) * ease;
}

bool BoardCamera::Moving() const
{
    return Length(m_goal.target - m_pose.target) > 1e-5f || std::abs(m_goal.yaw - m_pose.yaw) > 1e-4f || std::abs(m_goal.pitch - m_pose.pitch) > 1e-4f ||
           std::abs(m_goal.distance / m_pose.distance - 1) > 1e-4f || std::abs(m_goal.shift - m_pose.shift) > 1e-4f ||
           std::abs(m_goal.space - m_pose.space) > 1e-4f;
}

Vec3 BoardCamera::Eye() const
{
    const float horizontal = std::cos(m_pose.pitch);
    return m_pose.target + Vec3{std::sin(m_pose.yaw) * horizontal, std::sin(m_pose.pitch), std::cos(m_pose.yaw) * horizontal} * m_pose.distance;
}

Mat4 BoardCamera::View() const
{
    // Looking straight down, "up" on the screen points away from the eye's side of the board.
    const float yaw = m_pose.yaw, pitch = m_pose.pitch;
    const Vec3 up = Normalize(Vec3{-std::sin(yaw) * std::sin(pitch), std::cos(pitch), -std::cos(yaw) * std::sin(pitch)});
    return Mat4::LookAt(Eye(), m_pose.target, up);
}

Mat4 BoardCamera::Projection(float aspect) const
{
    const float widen = std::max(1.0f, MinAspect / (aspect * m_pose.space));
    Mat4 projection = Mat4::Perspective(2 * std::atan(std::tan(FieldOfView / 2) * widen), aspect, 0.01f, 20.0f);
    projection(0, 2) = -2 * m_pose.shift; // shift in normalized coordinates, which span 2
    return projection;
}

void BoardCamera::Ray(float x, float y, float width, float height, Vec3& origin, Vec3& direction) const
{
    const Mat4 inverse = (Projection(width / std::max(height, 1.0f)) * View()).Inverse();
    const float nx = x / width * 2 - 1, ny = 1 - y / height * 2;
    const Vec3 nearPoint = inverse.TransformPoint({nx, ny, -1});
    const Vec3 farPoint = inverse.TransformPoint({nx, ny, 1});
    origin = nearPoint;
    direction = Normalize(farPoint - nearPoint);
}
