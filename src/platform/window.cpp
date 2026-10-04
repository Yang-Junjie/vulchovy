#include "platform/window.h"

#include <stdexcept>

#include <GLFW/glfw3.h>

namespace vulchovy {

namespace {

Window* window_from(GLFWwindow* handle) {
    return static_cast<Window*>(glfwGetWindowUserPointer(handle));
}

void key_callback(GLFWwindow* handle, int key, int, int action, int) {
    if (Window* window = window_from(handle))
        window->input().on_key(key, action);
}

void mouse_button_callback(GLFWwindow* handle, int button, int action, int) {
    if (Window* window = window_from(handle))
        window->input().on_mouse_button(button, action);
}

void cursor_callback(GLFWwindow* handle, double x, double y) {
    if (Window* window = window_from(handle))
        window->input().on_cursor(x, y);
}

void scroll_callback(GLFWwindow* handle, double, double y_offset) {
    if (Window* window = window_from(handle))
        window->input().on_scroll(y_offset);
}

void glfw_error_callback(int error, const char* description) {
    (void)error;
    (void)description;
}

} // namespace

Window::Window(uint32_t width, uint32_t height, const char* title) {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
        throw std::runtime_error("GLFW: failed to initialize");

    if (!glfwVulkanSupported()) {
        glfwTerminate();
        throw std::runtime_error("GLFW: Vulkan is not supported on this system");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title, nullptr,
                               nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        throw std::runtime_error("GLFW: failed to create a window");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, key_callback);
    glfwSetMouseButtonCallback(window_, mouse_button_callback);
    glfwSetCursorPosCallback(window_, cursor_callback);
    glfwSetScrollCallback(window_, scroll_callback);
}

Window::~Window() {
    if (window_ != nullptr)
        glfwDestroyWindow(window_);
    glfwTerminate();
}

bool Window::should_close() const {
    return glfwWindowShouldClose(window_) != 0;
}

void Window::poll_events() {
    input_.new_frame();
    glfwPollEvents();
}

double Window::time() const {
    return glfwGetTime();
}

void Window::set_title(const char* title) const {
    glfwSetWindowTitle(window_, title);
}

vk::Extent2D Window::framebuffer_extent() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    return vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

std::vector<const char*> Window::required_extensions() const {
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    return std::vector<const char*>(extensions, extensions + count);
}

vk::SurfaceKHR Window::create_surface(vk::Instance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, window_, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("GLFW: failed to create the window surface");
    return vk::SurfaceKHR{surface};
}

} // namespace vulchovy
