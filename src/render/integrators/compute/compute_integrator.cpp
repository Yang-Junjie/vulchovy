#include "render/integrators/compute/compute_integrator.h"

#include <array>
#include <stdexcept>

#include <vulcao/command_buffer.h>
#include <vulcao/context.h>

namespace vulchovy::compute {

namespace {

constexpr uint32_t kThreadGroupSize = 8;

} // namespace

ComputeIntegrator::ComputeIntegrator(vulcao::Context& context,
                                     const std::filesystem::path& shader_path,
                                     const char* entry,
                                     size_t uniform_bytes,
                                     const SceneDescriptors& scene)
    : context_(context), scene_set_(scene.handle()) {
    shader_ = vulcao::ShaderModule::create_from_file(
        context_.device(), vk::ShaderStageFlagBits::eCompute, shader_path);

    // Derive the integrator's own set from the shader reflection.
    const std::vector<vk::DescriptorSetLayoutBinding>& bindings =
        shader_.reflection().bindings_for_set(1);
    if (bindings.empty())
        throw std::runtime_error("ComputeIntegrator: shader declares no set 1 bindings");
    integrator_bindings_ = bindings;
    uniform_binding_ = binding_for(vk::DescriptorType::eUniformBuffer);
    output_binding_ = binding_for(vk::DescriptorType::eStorageImage);

    set_layout_ = vulcao::DescriptorSetLayout::create(context_.device(), integrator_bindings_);
    pool_ =
        vulcao::DescriptorPool::create_for_bindings(context_.device(), integrator_bindings_, 1);
    set_ = pool_.allocate(set_layout_);

    const std::array<vk::DescriptorSetLayout, 2> set_layouts{scene.layout(), set_layout_.handle()};
    pipeline_layout_ = vulcao::PipelineLayout::create(context_.device(), set_layouts);
    pipeline_ = vulcao::Pipeline::create_compute(context_.device(), pipeline_layout_, shader_, entry);

    uniform_buffer_ = vulcao::Buffer::create(
        context_.allocator(), uniform_bytes, vk::BufferUsageFlagBits::eUniformBuffer,
        VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    create_output(context_.swapchain_extent());
    write_common_descriptors();
}

uint32_t ComputeIntegrator::binding_for(vk::DescriptorType type) const {
    uint32_t found = 0;
    bool present = false;
    for (const vk::DescriptorSetLayoutBinding& binding : integrator_bindings_) {
        if (binding.descriptorType != type)
            continue;
        if (present)
            throw std::runtime_error(
                "ComputeIntegrator: multiple set 1 bindings of the requested descriptor type");
        found = binding.binding;
        present = true;
    }
    if (!present)
        throw std::runtime_error(
            "ComputeIntegrator: no set 1 binding of the requested descriptor type");
    return found;
}

void ComputeIntegrator::create_output(vk::Extent2D extent) {
    output_ = vulcao::Image::create_2d(
        context_.allocator(), extent, vk::Format::eR32G32B32A32Sfloat,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
            vk::ImageUsageFlagBits::eTransferDst);
    output_view_ = vulcao::ImageView::create(context_.device(), output_);
}

void ComputeIntegrator::write_common_descriptors() {
    vulcao::DescriptorSetWriter{set_}
        .write_uniform_buffer(uniform_binding_, uniform_buffer_)
        .write_storage_image(output_binding_, output_view_, vk::ImageLayout::eGeneral)
        .flush();
}

void ComputeIntegrator::resize(vk::Extent2D extent) {
    create_output(extent);
    write_common_descriptors();
}

const vulcao::ImageView& ComputeIntegrator::output_view() const {
    return output_view_;
}

void ComputeIntegrator::write_uniforms(const void* data, size_t size) {
    uniform_buffer_.write_bytes(data, size);
}

void ComputeIntegrator::begin(vulcao::CommandBuffer& cmd) {
    cmd.transition(output_, vk::ImageLayout::eGeneral);
    cmd.bind_pipeline(pipeline_);
    cmd.bind_descriptor_sets(vk::PipelineBindPoint::eCompute, pipeline_layout_.handle(),
                             {scene_set_, set_.handle()});
}

void ComputeIntegrator::dispatch(vulcao::CommandBuffer& cmd, vk::Extent2D extent) {
    const uint32_t groups_x = (extent.width + kThreadGroupSize - 1) / kThreadGroupSize;
    const uint32_t groups_y = (extent.height + kThreadGroupSize - 1) / kThreadGroupSize;
    cmd.dispatch(groups_x, groups_y, 1);
}

void ComputeIntegrator::end(vulcao::CommandBuffer& cmd) {
    cmd.transition(output_, vk::ImageLayout::eShaderReadOnlyOptimal);
}

} // namespace vulchovy::compute
