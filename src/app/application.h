#pragma once

#include <cstdint>
#include <memory>

#include <vulcao/context.h>
#include <vulcao/frame_manager.h>

#include "camera/orbit_camera_controller.h"
#include "camera/perspective_camera.h"
#include "platform/window.h"
#include "render/display_pass.h"
#include "render/integrators/integrator.h"
#include "scene/gpu_scene.h"
#include "scene/scene.h"

namespace vulchovy {

/// @brief Owns the window, the Vulkan context, the camera/controller and the
///        statically selected integrator, and drives the frame loop.
class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void init_vulkan();
    void recreate_swapchain();
    bool update_camera(float dt);
    void update_window_title();
    void draw_frame();

    Window window_;
    vulcao::Context context_;

    // Built in init_vulkan, once the context is initialized.
    std::unique_ptr<vulcao::FrameManager> frame_manager_;
    std::unique_ptr<GpuScene> gpu_scene_;
    std::unique_ptr<DisplayPass> display_pass_;
    // The active integrator, chosen at compile time in init_vulkan.
    std::unique_ptr<Integrator> integrator_;

    std::unique_ptr<PerspectiveCamera> camera_;
    std::unique_ptr<OrbitCameraController> controller_;
    Scene scene_;

    uint32_t frame_index_ = 0;
    double last_time_ = 0.0;
};

} // namespace vulchovy
