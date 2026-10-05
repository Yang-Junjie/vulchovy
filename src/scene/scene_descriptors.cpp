#include "scene/scene_descriptors.h"

#include <array>
#include <vector>

#include "scene/gpu_scene.h"

namespace vulchovy {

namespace {

const std::array<vk::DescriptorSetLayoutBinding, 9> kBindings{{
    {0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {5, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute},
    {6, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {7, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
    {8, vk::DescriptorType::eCombinedImageSampler, kMaxSceneTextures,
     vk::ShaderStageFlagBits::eCompute},
}};

} // namespace

SceneDescriptors::SceneDescriptors(vk::Device device, const GpuScene& scene) {
    set_layout_ = vulcao::DescriptorSetLayout::create(device, kBindings);
    pool_ = vulcao::DescriptorPool::create_for_bindings(device, kBindings, 1);
    set_ = pool_.allocate(set_layout_);

    vulcao::DescriptorSetWriter writer{set_};
    writer.write_storage_buffer(0, scene.materials())
        .write_storage_buffer(1, scene.lights())
        .write_storage_buffer(2, scene.indices())
        .write_storage_buffer(3, scene.vertices())
        .write_storage_buffer(4, scene.instances())
        .write_image(5, scene.environment_view(), scene.environment_sampler(),
                     vk::ImageLayout::eShaderReadOnlyOptimal)
        .write_storage_buffer(6, scene.env_marginal())
        .write_storage_buffer(7, scene.env_conditional());

    // Texture array: fill every slot, using the 1x1 white fallback for the rest.
    const std::vector<vulcao::ImageView>& textures = scene.texture_views();
    for (uint32_t i = 0; i < kMaxSceneTextures; ++i) {
        const vulcao::ImageView& view =
            i < textures.size() ? textures[i] : scene.fallback_texture_view();
        writer.write_image(8, view, scene.texture_sampler(),
                           vk::ImageLayout::eShaderReadOnlyOptimal, i);
    }
    writer.flush();
}

} // namespace vulchovy
