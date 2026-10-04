#include "render/integrators/ray_query/debug_normals.h"

#include <filesystem>

#include <vulcao/command_buffer.h>

#include "camera/camera.h"
#include "scene/gpu_scene.h"

namespace vulchovy::ray_query {

namespace {

// Mirrors the `Uniforms` struct in shaders/integrators/ray_query/debug_normals.slang.
struct DebugUniforms {
    CameraData camera;
    glm::uvec4 options; // x = width, y = height, z = frame
};

static_assert(sizeof(DebugUniforms) == 80, "DebugUniforms must match the shader block");

} // namespace

DebugNormals::DebugNormals(vulcao::Context& context, const GpuScene& scene)
    : RayQueryIntegrator(context, scene,
                         std::filesystem::path{VULCHOVY_SHADER_DIR} / "debug_normals.comp.spv",
                         "computeMain", sizeof(DebugUniforms)) {}

void DebugNormals::record(const FrameContext& frame) {
    DebugUniforms uniforms{};
    uniforms.camera = frame.camera.gpu_data();
    uniforms.options = glm::uvec4(frame.extent.width, frame.extent.height, frame.frame_index, 0);
    write_uniforms(&uniforms, sizeof(uniforms));

    vulcao::CommandBuffer& cmd = frame.command_buffer;
    begin(cmd);
    dispatch(cmd, frame.extent);
    end(cmd);
}

} // namespace vulchovy::ray_query
