#include "render/display_pass.h"

#include <array>
#include <filesystem>

#include <vulcao/command_buffer.h>
#include <vulcao/context.h>
#include <vulcao/image_view.h>
#include <vulcao/rendering.h>

namespace vulchovy {

DisplayPass::DisplayPass(vulcao::Context& context) : context_(context) {
    const std::filesystem::path shader_dir{VULCHOVY_SHADER_DIR};
    vertex_shader_ = vulcao::ShaderModule::create_from_file(
        context_.device(), vk::ShaderStageFlagBits::eVertex, shader_dir / "display.vert.spv");
    fragment_shader_ = vulcao::ShaderModule::create_from_file(
        context_.device(), vk::ShaderStageFlagBits::eFragment, shader_dir / "display.frag.spv");

    sampler_ = vulcao::Sampler::linear(context_.device(), false);

    const std::array<vk::DescriptorSetLayoutBinding, 1> bindings{{
        {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
    }};
    set_layout_ = vulcao::DescriptorSetLayout::create(context_.device(), bindings);
    pool_ = vulcao::DescriptorPool::create_for_bindings(context_.device(), bindings, 1);
    set_ = pool_.allocate(set_layout_);

    const std::array<vk::DescriptorSetLayout, 1> set_layouts{set_layout_.handle()};
    pipeline_layout_ = vulcao::PipelineLayout::create(context_.device(), set_layouts);

    vulcao::GraphicsPipelineInfo info;
    info.vertex_shader = vertex_shader_.handle();
    info.fragment_shader = fragment_shader_.handle();
    info.vertex_entry = "vertexMain";
    info.fragment_entry = "fragmentMain";
    info.color_formats = {context_.swapchain_format()};
    info.cull_mode = {};
    pipeline_ = vulcao::Pipeline::create_graphics(context_.device(), pipeline_layout_, info);
}

void DisplayPass::set_input(const vulcao::ImageView& view) {
    vulcao::DescriptorSetWriter{set_}
        .write_image(0, view, sampler_, vk::ImageLayout::eShaderReadOnlyOptimal)
        .flush();
}

void DisplayPass::record(vulcao::CommandBuffer& cmd,
                         vk::Image swapchain_image,
                         vk::ImageView swapchain_view,
                         vk::Extent2D extent) {
    const vk::ClearColorValue clear{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};

    cmd.transition_to_render(swapchain_image);
    cmd.begin_rendering(
        extent, vulcao::color_attachment(swapchain_view, vk::ImageLayout::eColorAttachmentOptimal,
                                         clear));
    cmd.bind_pipeline(pipeline_);
    cmd.set_viewport(extent);
    cmd.set_scissor(extent);
    cmd.bind_descriptor_sets(vk::PipelineBindPoint::eGraphics, pipeline_layout_.handle(),
                             {set_.handle()});
    cmd.draw(3);
    cmd.end_rendering();
    cmd.transition_to_present(swapchain_image);
}

} // namespace vulchovy
