#include "app/application.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <GLFW/glfw3.h>

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

Application::Application(std::filesystem::path scene_path)
    : window_(1280, 720, "vulchovy"), context_(make_context_info(window_)),
      camera_(std::make_unique<PerspectiveCamera>()), scene_path_(std::move(scene_path)) {
    load_scene(scene_path_);
    init_vulkan();
    last_time_ = window_.time();
}

void Application::load_scene(const std::filesystem::path& path) {
    description_ = default_scene_description();

    bool loaded = false;
    bool is_scene_file = false;
    if (!path.empty()) {
        const std::string extension = path.extension().string();
        is_scene_file = extension == ".json" || extension == ".scene";
        if (is_scene_file) {
            description_ = load_scene_description(path);
        } else {
            ModelPlacement placement;
            placement.path = path.string();
            placement.name = path.stem().string();
            description_.models.push_back(std::move(placement));
        }
        scene_ = build_scene(description_);
        loaded = !scene_.nodes.empty();
    }

    if (!loaded) {
        scene_ = make_default_scene();
        description_ = default_scene_description();
        return;
    }

    const Aabb bounds = scene_bounds(scene_);
    if (is_scene_file) {
        // A scene file stores its own camera.
        camera_->set_pose(description_.camera.position, description_.camera.target,
                          description_.camera.up);
        camera_->set_vertical_fov(description_.camera.vertical_fov_degrees);
    } else if (bounds.valid()) {
        // Frame a freshly imported model with the camera before the orbit
        // controller is built from the current pose.
        const vec3 center = bounds.center();
        const float radius = std::max(bounds.radius(), 0.05f);
        camera_->set_pose(center + vec3{0.0f, 0.6f * radius, 2.8f * radius}, center);
        description_.camera.position = camera_->position();
        description_.camera.target = center;
    }
}

void Application::save_scene() {
    description_.camera.position = camera_->position();
    description_.camera.target = camera_->target();
    description_.camera.up = camera_->up();
    description_.camera.vertical_fov_degrees = camera_->vertical_fov_degrees();

    std::filesystem::path out = scene_path_;
    if (out.empty() || out.extension() != ".json")
        out = "scene.json";
    if (save_scene_description(description_, out))
        std::fprintf(stderr, "vulchovy: saved scene to '%s'\n", out.string().c_str());
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

        if (window_.input().key_pressed(GLFW_KEY_F5))
            save_scene();

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
    std::string title = "vulchovy - " + std::string(integrator_->name());
    if (!scene_path_.empty())
        title += " - " + scene_path_.filename().string();
    window_.set_title(title.c_str());
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
