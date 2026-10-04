#pragma once

#include <vulkan/vulkan.hpp>

#include <vulcao/descriptor_set.h>

namespace vulchovy {

class GpuScene;

/// @brief Descriptor set (set 0) shared by every integrator: the scene buffers
///        (materials, lights, indices, vertices, instances).
///
/// Backend-specific bindings such as the acceleration structure live in the
/// integrator's own set, so this layout is identical for the ray query and the
/// software compute backends.
class SceneDescriptors {
public:
    SceneDescriptors() = default;

    SceneDescriptors(vk::Device device, const GpuScene& scene);

    SceneDescriptors(SceneDescriptors&&) noexcept = default;
    SceneDescriptors& operator=(SceneDescriptors&&) noexcept = default;
    SceneDescriptors(const SceneDescriptors&) = delete;
    SceneDescriptors& operator=(const SceneDescriptors&) = delete;

    vk::DescriptorSetLayout layout() const { return set_layout_.handle(); }
    vk::DescriptorSet handle() const { return set_.handle(); }
    bool valid() const { return set_layout_.valid(); }

private:
    vulcao::DescriptorSetLayout set_layout_;
    vulcao::DescriptorPool pool_;
    vulcao::DescriptorSet set_;
};

} // namespace vulchovy
