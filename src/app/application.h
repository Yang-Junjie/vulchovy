#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

#include <vulcao/context.h>
#include <vulcao/frame_manager.h>

#include "camera/fps_camera_controller.h"
#include "camera/perspective_camera.h"
#include "io/scene_io.h"
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
    /// @param scene_path Optional `.json` scene or `.obj` model to load; the
    ///        built-in demo is used when empty.
    explicit Application(std::filesystem::path scene_path = {});
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
    void load_scene(const std::filesystem::path& path);
    void save_scene();

    Window window_;
    vulcao::Context context_;

    // Built in init_vulkan, once the context is initialized.
    std::unique_ptr<vulcao::FrameManager> frame_manager_;
    std::unique_ptr<GpuScene> gpu_scene_;
    std::unique_ptr<DisplayPass> display_pass_;
    // The active integrator, chosen at compile time in init_vulkan.
    std::unique_ptr<Integrator> integrator_;

    std::unique_ptr<PerspectiveCamera> camera_;
    std::unique_ptr<FpsCameraController> controller_;
    Scene scene_;
    SceneDescription description_;
    std::filesystem::path scene_path_;

    uint32_t frame_index_ = 0;
    double last_time_ = 0.0;
};

} // namespace vulchovy
