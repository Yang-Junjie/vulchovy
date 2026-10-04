#include "camera/orbit_camera_controller.h"

#include <cmath>

#include "camera/perspective_camera.h"
#include "platform/input.h"

namespace vulchovy {

namespace {

constexpr int kMouseLeft = 0;
constexpr int kMouseRight = 1;
constexpr int kMouseMiddle = 2;

} // namespace

OrbitCameraController::OrbitCameraController(PerspectiveCamera& camera) : camera_(camera) {
    target_ = camera_.target();
    const vec3 offset = camera_.position() - target_;
    distance_ = length(offset);

    if (distance_ > 0.0001f) {
        pitch_ = std::asin(glm::clamp(offset.y / distance_, -1.0f, 1.0f));
        yaw_ = std::atan2(offset.x, offset.z);
    }
}

bool OrbitCameraController::update(const InputState& input, float dt) {
    (void)dt;

    const vec2 delta = input.mouse_delta();
    bool changed = false;

    const bool rotating = input.mouse_down(kMouseLeft);
    const bool panning = input.mouse_down(kMouseRight) || input.mouse_down(kMouseMiddle);

    if (rotating && delta != vec2{0.0f}) {
        yaw_ -= delta.x * rotate_speed_;
        pitch_ = glm::clamp(pitch_ + delta.y * rotate_speed_, -1.55334f, 1.55334f);
        changed = true;
    } else if (panning && delta != vec2{0.0f}) {
        const float scale = distance_ * pan_speed_;
        target_ -= camera_.right() * (delta.x * scale);
        target_ += camera_.true_up() * (delta.y * scale);
        changed = true;
    }

    const float scroll = input.scroll_delta();
    if (scroll != 0.0f) {
        distance_ = glm::clamp(distance_ * std::exp(-scroll * zoom_speed_), 0.2f, 200.0f);
        changed = true;
    }

    if (changed)
        apply();
    return changed;
}

void OrbitCameraController::apply() {
    const float cos_pitch = std::cos(pitch_);
    const vec3 offset{distance_ * cos_pitch * std::sin(yaw_), distance_ * std::sin(pitch_),
                      distance_ * cos_pitch * std::cos(yaw_)};
    camera_.set_pose(target_ + offset, target_, vec3{0.0f, 1.0f, 0.0f});
}

} // namespace vulchovy
