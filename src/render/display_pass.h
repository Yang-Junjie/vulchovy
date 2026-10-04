#pragma once

#include <vulcao/descriptor_set.h>
#include <vulcao/pipeline.h>
#include <vulcao/pipeline_layout.h>
#include <vulcao/sampler.h>
#include <vulcao/shader_module.h>

namespace vulcao {
class CommandBuffer;
class Context;
class ImageView;
} // namespace vulcao

namespace vulchovy {

/// @brief Fullscreen display pass.
///
/// Samples the linear HDR image produced by the active integrator, tonemaps it
/// and writes a fullscreen triangle into the swapchain image.
class DisplayPass {
public:
    explicit DisplayPass(vulcao::Context& context);

    DisplayPass(const DisplayPass&) = delete;
    DisplayPass& operator=(const DisplayPass&) = delete;

    /// @brief Points the pass at the HDR image to sample.
    /// @warning The caller must ensure the GPU is idle.
    void set_input(const vulcao::ImageView& view);

    /// @brief Records the display draw into an acquired swapchain image.
    void record(vulcao::CommandBuffer& cmd,
                vk::Image swapchain_image,
                vk::ImageView swapchain_view,
                vk::Extent2D extent);

private:
    vulcao::Context& context_;

    vulcao::ShaderModule vertex_shader_;
    vulcao::ShaderModule fragment_shader_;
    vulcao::Sampler sampler_;
    vulcao::DescriptorSetLayout set_layout_;
    vulcao::DescriptorPool pool_;
    vulcao::DescriptorSet set_;
    vulcao::PipelineLayout pipeline_layout_;
    vulcao::Pipeline pipeline_;
};

} // namespace vulchovy
