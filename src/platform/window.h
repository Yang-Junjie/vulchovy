#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "platform/input.h"

struct GLFWwindow;

namespace vulchovy {

/// @brief RAII window backed by GLFW.
///
/// Owns the GLFW library lifetime and the platform window, creates the
/// presentation surface and forwards keyboard/mouse events into an InputState.
class Window {
public:
    Window(uint32_t width, uint32_t height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool should_close() const;

    /// @brief Starts a new input frame and polls window events.
    void poll_events();

    /// @brief Seconds since GLFW was initialized.
    double time() const;

    void set_title(const char* title) const;

    vk::Extent2D framebuffer_extent() const;

    InputState& input() { return input_; }
    const InputState& input() const { return input_; }

    /// @brief Instance extensions required to present to this window.
    std::vector<const char*> required_extensions() const;

    /// @brief Creates a presentation surface for an instance.
    vk::SurfaceKHR create_surface(vk::Instance instance) const;

    GLFWwindow* handle() const { return window_; }

private:
    GLFWwindow* window_ = nullptr;
    InputState input_;
};

} // namespace vulchovy
