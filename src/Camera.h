#pragma once

#include "MathTypes.h"

enum class CameraMode { Overview, Top };
constexpr int CameraModeCount = 2;

// A camera circling a point over the board. Its moves are eased: dragging, zooming and the views set where it heads.
// The overview looks at the board from White's side at an angle; the other view from straight above.
class BoardCamera {
public:
    BoardCamera();
    void SetMode(CameraMode mode);
    CameraMode Mode() const { return m_mode; }
    void Reset();
    void Rotate(float yawRadians, float pitchRadians);
    void Zoom(float steps);
    // The share of the window's width left for the board beside the panel: the view centers on it, and widens
    // when it is narrow, so the whole board stays in sight.
    void SetSpace(float share)
    {
        m_goal.shift = (share - 1) / 2;
        m_goal.space = share;
    }

    void Update(float seconds);
    bool Moving() const;

    math::Vec3 Eye() const;
    math::Mat4 View() const;
    math::Mat4 Projection(float aspect) const;
    // The ray through a point of the window, in pixels from the top left.
    void Ray(float x, float y, float width, float height, math::Vec3& origin, math::Vec3& direction) const;

private:
    struct Pose {
        math::Vec3 target;
        float yaw = 0, pitch = 1, distance = 1;
        float shift = 0, space = 1;
    };

    void ApplyMode();

    CameraMode m_mode = CameraMode::Overview;
    Pose m_pose, m_goal;
};
