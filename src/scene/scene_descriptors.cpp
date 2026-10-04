#include "scene/scene_descriptors.h"

#include <array>

#include "scene/gpu_scene.h"

namespace vulchovy {

namespace {

const std::array<vk::DescriptorSetLayoutBinding, 5> kBindings{{
    {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
}};

} // namespace

SceneDescriptors::SceneDescriptors(vk::Device device, const GpuScene& scene) {
    set_layout_ = vulcao::DescriptorSetLayout::create(device, kBindings);
    pool_ = vulcao::DescriptorPool::create_for_bindings(device, kBindings, 1);
    set_ = pool_.allocate(set_layout_);

    vulcao::DescriptorSetWriter{set_}
        .write_storage_buffer(0, scene.materials())
        .write_storage_buffer(1, scene.lights())
        .write_storage_buffer(2, scene.indices())
        .write_storage_buffer(3, scene.vertices())
        .write_storage_buffer(4, scene.instances())
        .flush();
}

} // namespace vulchovy
