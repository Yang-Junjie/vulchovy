#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include <vulcao/buffer.h>
#include <vulcao/descriptor_set.h>
#include <vulcao/image.h>
#include <vulcao/image_view.h>
#include <vulcao/pipeline.h>
#include <vulcao/pipeline_layout.h>
#include <vulcao/shader_module.h>

#include "render/integrators/integrator.h"
#include "scene/scene_descriptors.h"

namespace vulcao {
class Context;
} // namespace vulcao

namespace vulchovy::compute {

/// @brief Shared implementation of an integrator recorded as a compute dispatch.
///
/// The integrator's own descriptor set (set 1) is derived from the shader
/// reflection: the uniform buffer and output image bindings are located by
/// descriptor type, so concrete integrators never write a binding list by hand.
/// Set 0 is the shared scene set owned by the GpuScene.
class ComputeIntegrator : public Integrator {
public:
    ComputeIntegrator(vulcao::Context& context,
                      const std::filesystem::path& shader_path,
                      const char* entry,
                      size_t uniform_bytes,
                      const SceneDescriptors& scene);

    void resize(vk::Extent2D extent) override;
    const vulcao::ImageView& output_view() const override;

protected:
    /// @brief Uploads this frame's uniform block.
    void write_uniforms(const void* data, size_t size);

    /// @brief Transitions the output to storage and binds the scene and integrator sets.
    void begin(vulcao::CommandBuffer& cmd);

    /// @brief Dispatches one thread group per tile of the extent.
    void dispatch(vulcao::CommandBuffer& cmd, vk::Extent2D extent);

    /// @brief Transitions the output to a sampled layout.
    void end(vulcao::CommandBuffer& cmd);

    /// @brief The integrator's own descriptor set (set 1).
    vulcao::DescriptorSet& descriptor_set() { return set_; }

    /// @brief The HDR output image, for integrators that read or clear it.
    vulcao::Image& output_image() { return output_; }

    /// @brief Returns the binding number in set 1 whose descriptor type is
    ///        @p type, or throws if it is absent or ambiguous.
    uint32_t binding_for(vk::DescriptorType type) const;

    vulcao::Context& context_;

private:
    void create_output(vk::Extent2D extent);
    void write_common_descriptors();

    uint32_t uniform_binding_ = 0;
    uint32_t output_binding_ = 0;
    vk::DescriptorSet scene_set_;
    std::vector<vk::DescriptorSetLayoutBinding> integrator_bindings_;
    vulcao::ShaderModule shader_;
    vulcao::DescriptorSetLayout set_layout_;
    vulcao::DescriptorPool pool_;
    vulcao::DescriptorSet set_;
    vulcao::PipelineLayout pipeline_layout_;
    vulcao::Pipeline pipeline_;
    vulcao::Buffer uniform_buffer_;

    vulcao::Image output_;
    vulcao::ImageView output_view_;
};

} // namespace vulchovy::compute
