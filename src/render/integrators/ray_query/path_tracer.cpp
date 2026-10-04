#include "render/integrators/ray_query/path_tracer.h"

#include <array>
#include <filesystem>

#include <vulcao/command_buffer.h>

#include "camera/camera.h"
#include "scene/gpu_scene.h"

namespace vulchovy::ray_query {

namespace {

// Mirrors the `Uniforms` struct in shaders/integrators/ray_query/path_tracer.slang.
struct PathTracerUniforms {
    CameraData camera;
    glm::uvec4 counts;  // x = material count, y = light count, z = instance count, w = frame
    glm::uvec4 options; // x = max depth, y = width, z = height, w = sample count
};

static_assert(sizeof(PathTracerUniforms) == 96, "PathTracerUniforms must match the shader block");

} // namespace

PathTracer::PathTracer(vulcao::Context& context, const GpuScene& scene)
    : RayQueryIntegrator(context, scene,
                         std::filesystem::path{VULCHOVY_SHADER_DIR} / "path_tracer.comp.spv",
                         "computeMain", sizeof(PathTracerUniforms)) {}

void PathTracer::reset() {
    sample_count_ = 0;
    needs_clear_ = true;
}

void PathTracer::record(const FrameContext& frame) {
    PathTracerUniforms uniforms{};
    uniforms.camera = frame.camera.gpu_data();
    uniforms.counts = glm::uvec4(scene_.material_count(), scene_.light_count(),
                                 scene_.instance_count(), frame.frame_index);
    uniforms.options = glm::uvec4(max_depth_, frame.extent.width, frame.extent.height, sample_count_);
    write_uniforms(&uniforms, sizeof(uniforms));

    vulcao::CommandBuffer& cmd = frame.command_buffer;
    begin(cmd);

    if (needs_clear_) {
        const vk::ImageSubresourceRange range{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        const vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        cmd.clear_color_image(output_image().handle(), vk::ImageLayout::eGeneral, clear, range);
        needs_clear_ = false;
    }

    dispatch(cmd, frame.extent);
    end(cmd);

    ++sample_count_;
}

} // namespace vulchovy::ray_query
