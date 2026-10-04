#include "render/integrators/compute/whitted.h"

#include <filesystem>

#include <vulcao/command_buffer.h>

#include "render/integrators/whitted_uniforms.h"
#include "scene/gpu_scene.h"

namespace vulchovy::compute {

Whitted::Whitted(vulcao::Context& context, const GpuScene& scene)
    : ComputeIntegrator(context,
                        std::filesystem::path{VULCHOVY_SHADER_DIR} / "whitted_compute.comp.spv",
                        "computeMain", sizeof(Uniforms), scene.scene_descriptors()),
      scene_(scene) {}

void Whitted::record(const FrameContext& frame) {
    Uniforms uniforms{};
    uniforms.camera = frame.camera.gpu_data();
    uniforms.counts = glm::uvec4(scene_.material_count(), scene_.light_count(),
                                 scene_.instance_count(), frame.frame_index);
    uniforms.options = glm::uvec4(max_depth_, frame.extent.width, frame.extent.height, 0);
    write_uniforms(&uniforms, sizeof(uniforms));

    vulcao::CommandBuffer& cmd = frame.command_buffer;
    begin(cmd);
    dispatch(cmd, frame.extent);
    end(cmd);
}

} // namespace vulchovy::compute
