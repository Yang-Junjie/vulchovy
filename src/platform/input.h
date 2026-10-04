#pragma once

#include <array>

#include "core/math.h"

namespace vulchovy {

/// @brief Keyboard and mouse state for one frame.
///
/// Callbacks fill the held/pressed flags and the per-frame deltas; new_frame()
/// is called once before polling to clear the previous frame's edges and deltas.
class InputState {
public:
    static constexpr int kPress = 1;   // GLFW_PRESS
    static constexpr int kRelease = 0; // GLFW_RELEASE

    /// @brief Clears per-frame edges and deltas. Call before polling events.
    void new_frame();

    void on_key(int key, int action);
    void on_mouse_button(int button, int action);
    void on_cursor(double x, double y);
    void on_scroll(double y_offset);

    bool key_down(int key) const;
    bool key_pressed(int key) const;
    bool mouse_down(int button) const;

    vec2 mouse_delta() const { return mouse_delta_; }
    float scroll_delta() const { return scroll_delta_; }

private:
    static constexpr int kMaxKeys = 512;
    static constexpr int kMaxButtons = 16;

    std::array<bool, kMaxKeys> key_down_{};
    std::array<bool, kMaxKeys> key_pressed_{};
    std::array<bool, kMaxButtons> mouse_down_{};

    bool cursor_initialized_ = false;
    double last_x_ = 0.0;
    double last_y_ = 0.0;
    vec2 mouse_delta_{0.0f};
    float scroll_delta_ = 0.0f;
};

} // namespace vulchovy
