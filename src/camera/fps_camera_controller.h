#pragma once

#include "camera/camera_controller.h"
#include "core/math.h"

namespace vulchovy {

class PerspectiveCamera;

/// @brief First-person (FPS) controller.
///
/// Hold the right mouse button to look around; the cursor is not captured. Walk
/// with W/A/S/D, fly up/down with E/Q (or Space/Left Ctrl), hold Shift to sprint
/// and scroll to change the movement speed.
class FpsCameraController : public CameraController {
public:
    explicit FpsCameraController(PerspectiveCamera& camera);

    bool update(const InputState& input, float dt) override;

private:
    void apply();

    PerspectiveCamera& camera_;
    vec3 position_{0.0f};
    float yaw_ = 0.0f;   // radians
    float pitch_ = 0.0f; // radians

    float look_speed_ = 0.003f;   // radians per pixel
    float move_speed_ = 3.0f;     // units per second
    float sprint_multiplier_ = 4.0f;
    float speed_scroll_speed_ = 0.15f; // per scroll tick
};

} // namespace vulchovy
