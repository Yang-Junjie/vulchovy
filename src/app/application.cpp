#include "app/application.h"

#include <string>

#include "render/integrators/compute/whitted.h"
#include "render/integrators/ray_query/path_tracer.h"

namespace vulchovy {

namespace {

/// @brief Which rendering backend the application is built with.
enum class Backend {
    hardware_ray_query, ///< Ray query compute kernel; needs the ray query feature.
    software_compute,   ///< Pure compute software traversal; runs without hardware ray tracing.
};

// Compile-time backend selection. With software_compute the context does not
// request the ray query feature and GpuScene skips the acceleration structures.
constexpr Backend kBackend = Backend::hardware_ray_query;
constexpr bool kHardwareRayTracing = kBackend == Backend::hardware_ray_query;

vulcao::ContextInfo make_context_info(const Window& window) {
    vulcao::ContextInfo info;
    info.app_name = "vulchovy";
    info.extensions = window.required_extensions();
#ifdef VULCHOVY_VALIDATION
    info.validation = VULCHOVY_VALIDATION;
#endif
    // Ray queries imply acceleration structures and buffer device addresses.
    info.device_features.ray_query = kHardwareRayTracing;
    // The display vertex shader derives positions from SV_VertexID.
    info.device_features.shader_draw_parameters = true;
    return info;
}

// Compile-time integrator selection, tied to the backend above.
std::unique_ptr<Integrator> make_integrator(vulcao::Context& context, const GpuScene& scene) {
    if constexpr (kBackend == Backend::hardware_ray_query)
        return std::make_unique<ray_query::PathTracer>(context, scene);
    else
        return std::make_unique<compute::Whitted>(context, scene);
}

} // namespace

Application::Application()
    : window_(1280, 720, "vulchovy"), context_(make_context_info(window_)),
      camera_(std::make_unique<PerspectiveCamera>()), scene_(make_default_scene()) {
    init_vulkan();
    last_time_ = window_.time();
}

Application::~Application() {
    if (context_.initialized())
        context_.wait_idle();
}

void Application::init_vulkan() {
    const vk::SurfaceKHR surface = window_.create_surface(context_.instance());
    context_.initialize(surface, window_.framebuffer_extent());

    frame_manager_ = std::make_unique<vulcao::FrameManager>(context_);
    gpu_scene_ = std::make_unique<GpuScene>(context_, scene_, kHardwareRayTracing);

    integrator_ = make_integrator(context_, *gpu_scene_);
    display_pass_ = std::make_unique<DisplayPass>(context_);
    display_pass_->set_input(integrator_->output_view());

    controller_ = std::make_unique<OrbitCameraController>(*camera_);

    update_window_title();
}

void Application::run() {
    while (!window_.should_close()) {
        window_.poll_events();

        const vk::Extent2D extent = window_.framebuffer_extent();
        if (extent.width == 0 || extent.height == 0)
            continue; // Minimized.
        if (extent != context_.swapchain_extent())
            recreate_swapchain();

        const double now = window_.time();
        const float dt = static_cast<float>(now - last_time_);
        last_time_ = now;
        if (update_camera(dt))
            integrator_->reset();

        draw_frame();
        ++frame_index_;
    }
    context_.wait_idle();
}

void Application::recreate_swapchain() {
    vk::Extent2D extent = window_.framebuffer_extent();
    if (extent.width == 0 || extent.height == 0)
        extent = context_.swapchain_extent();

    context_.wait_idle();
    frame_manager_->recreate_swapchain(extent);
    integrator_->resize(extent);
    integrator_->reset();
    display_pass_->set_input(integrator_->output_view());
}

bool Application::update_camera(float dt) {
    return controller_->update(window_.input(), dt);
}

void Application::update_window_title() {
    window_.set_title(("vulchovy - " + std::string(integrator_->name())).c_str());
}

void Application::draw_frame() {
    vulcao::Frame frame;
    try {
        frame = frame_manager_->begin_frame();
    } catch (const vk::OutOfDateKHRError&) {
        recreate_swapchain();
        return;
    }

    const vk::Extent2D extent = context_.swapchain_extent();
    camera_->set_aspect(static_cast<float>(extent.width) / static_cast<float>(extent.height));

    const FrameContext frame_context{*camera_, *frame.command_buffer, extent, frame_index_,
                                     static_cast<float>(window_.time())};
    integrator_->record(frame_context);

    display_pass_->record(*frame.command_buffer, context_.swapchain_images()[frame.image_index],
                          context_.swapchain_image_views()[frame.image_index], extent);

    frame_manager_->end_frame(frame);

    try {
        if (!frame_manager_->present(frame))
            recreate_swapchain();
    } catch (const vk::OutOfDateKHRError&) {
        recreate_swapchain();
    }
}

} // namespace vulchovy
