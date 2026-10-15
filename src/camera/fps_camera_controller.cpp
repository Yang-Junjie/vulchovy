#include "camera/fps_camera_controller.h"

#include <cmath>

#include <GLFW/glfw3.h>

#include "camera/perspective_camera.h"
#include "platform/input.h"

namespace vulchovy {

namespace {

constexpr int kMouseRight = 1;
constexpr float kPitchLimit = 1.55334f; // ~89 degrees
const vec3 kWorldUp{0.0f, 1.0f, 0.0f};

} // namespace

FpsCameraController::FpsCameraController(PerspectiveCamera& camera) : camera_(camera) {
    position_ = camera_.position();

    const vec3 forward = normalize(camera_.target() - camera_.position());
    pitch_ = std::asin(glm::clamp(forward.y, -1.0f, 1.0f));
    yaw_ = std::atan2(forward.x, -forward.z);
}

bool FpsCameraController::update(const InputState& input, float dt) {
    bool changed = false;

    if (input.mouse_down(kMouseRight)) {
        const vec2 delta = input.mouse_delta();
        if (delta != vec2{0.0f}) {
            yaw_ += delta.x * look_speed_;
            pitch_ = glm::clamp(pitch_ - delta.y * look_speed_, -kPitchLimit, kPitchLimit);
            changed = true;
        }
    }

    const float scroll = input.scroll_delta();
    if (scroll != 0.0f) {
        move_speed_ =
            glm::clamp(move_speed_ * std::exp(scroll * speed_scroll_speed_), 0.1f, 100.0f);
    }

    const float cos_pitch = std::cos(pitch_);
    const vec3 forward{std::sin(yaw_) * cos_pitch, std::sin(pitch_), -std::cos(yaw_) * cos_pitch};
    const vec3 horizontal = normalize(vec3{forward.x, 0.0f, forward.z});
    const vec3 right = normalize(cross(horizontal, kWorldUp));

    vec3 move{0.0f};
    if (input.key_down(GLFW_KEY_W))
        move += horizontal;
    if (input.key_down(GLFW_KEY_S))
        move -= horizontal;
    if (input.key_down(GLFW_KEY_D))
        move += right;
    if (input.key_down(GLFW_KEY_A))
        move -= right;
    if (input.key_down(GLFW_KEY_E) || input.key_down(GLFW_KEY_SPACE))
        move += kWorldUp;
    if (input.key_down(GLFW_KEY_Q) || input.key_down(GLFW_KEY_LEFT_CONTROL))
        move -= kWorldUp;

    if (move != vec3{0.0f}) {
        float speed = move_speed_;
        if (input.key_down(GLFW_KEY_LEFT_SHIFT) || input.key_down(GLFW_KEY_RIGHT_SHIFT))
            speed *= sprint_multiplier_;
        position_ += normalize(move) * (speed * dt);
        changed = true;
    }

    if (changed)
        apply();
    return changed;
}

void FpsCameraController::apply() {
    const float cos_pitch = std::cos(pitch_);
    const vec3 forward{std::sin(yaw_) * cos_pitch, std::sin(pitch_), -std::cos(yaw_) * cos_pitch};
    camera_.set_pose(position_, position_ + forward, kWorldUp);
}

} // namespace vulchovy
