#include "platform/input.h"

namespace vulchovy {

void InputState::new_frame() {
    key_pressed_.fill(false);
    mouse_delta_ = vec2{0.0f};
    scroll_delta_ = 0.0f;
}

void InputState::on_key(int key, int action) {
    if (key < 0 || key >= kMaxKeys)
        return;
    if (action == kPress) {
        key_down_[static_cast<size_t>(key)] = true;
        key_pressed_[static_cast<size_t>(key)] = true;
    } else if (action == kRelease) {
        key_down_[static_cast<size_t>(key)] = false;
    }
}

void InputState::on_mouse_button(int button, int action) {
    if (button < 0 || button >= kMaxButtons)
        return;
    mouse_down_[static_cast<size_t>(button)] = (action != kRelease);
}

void InputState::on_cursor(double x, double y) {
    if (!cursor_initialized_) {
        last_x_ = x;
        last_y_ = y;
        cursor_initialized_ = true;
        return;
    }

    mouse_delta_ += vec2{static_cast<float>(x - last_x_), static_cast<float>(y - last_y_)};
    last_x_ = x;
    last_y_ = y;
}

void InputState::on_scroll(double y_offset) {
    scroll_delta_ += static_cast<float>(y_offset);
}

bool InputState::key_down(int key) const {
    return key >= 0 && key < kMaxKeys && key_down_[static_cast<size_t>(key)];
}

bool InputState::key_pressed(int key) const {
    return key >= 0 && key < kMaxKeys && key_pressed_[static_cast<size_t>(key)];
}

bool InputState::mouse_down(int button) const {
    return button >= 0 && button < kMaxButtons && mouse_down_[static_cast<size_t>(button)];
}

} // namespace vulchovy
