#pragma once

#include "camera/camera_controller.h"
#include "core/math.h"

namespace vulchovy {

class PerspectiveCamera;

/// @brief Orbit controller: drag to rotate around a target, drag with the middle
///        or right button to pan, scroll to dolly.
class OrbitCameraController : public CameraController {
public:
    explicit OrbitCameraController(PerspectiveCamera& camera);

    bool update(const InputState& input, float dt) override;

private:
    void apply();

    PerspectiveCamera& camera_;
    vec3 target_{0.0f};
    float distance_ = 5.0f;
    float yaw_ = 0.0f;   // radians
    float pitch_ = 0.0f; // radians

    float rotate_speed_ = 0.005f; // radians per pixel
    float pan_speed_ = 0.0015f;   // fraction of the distance per pixel
    float zoom_speed_ = 0.12f;    // per scroll tick
};

} // namespace vulchovy
