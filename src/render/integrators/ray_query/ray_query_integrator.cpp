#include "render/integrators/ray_query/ray_query_integrator.h"

#include <vulcao/context.h>

#include "scene/gpu_scene.h"

namespace vulchovy::ray_query {

RayQueryIntegrator::RayQueryIntegrator(vulcao::Context& context,
                                       const GpuScene& scene,
                                       const std::filesystem::path& shader_path,
                                       const char* entry,
                                       size_t uniform_bytes)
    : compute::ComputeIntegrator(context, shader_path, entry, uniform_bytes,
                                 scene.scene_descriptors()),
      scene_(scene) {
    vulcao::DescriptorSetWriter{descriptor_set()}
        .write_acceleration_structure(
            binding_for(vk::DescriptorType::eAccelerationStructureKHR), scene_.tlas())
        .flush();
}

} // namespace vulchovy::ray_query
