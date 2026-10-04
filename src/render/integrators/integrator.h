#pragma once

#include <cstdint>

#include <vulkan/vulkan.hpp>

#include <vulcao/image_view.h>

namespace vulcao {
class CommandBuffer;
} // namespace vulcao

namespace vulchovy {

class Camera;

/// @brief Per-frame inputs an integrator needs to record its work.
struct FrameContext {
    const Camera& camera;
    vulcao::CommandBuffer& command_buffer;
    vk::Extent2D extent;
    uint32_t frame_index = 0;
    float time = 0.0f;
};

/// @brief Abstract rendering algorithm.
///
/// The integrator owns its pipeline and output image, records its work into the
/// frame command buffer and exposes the HDR result as a sampled view. Swapping
/// algorithms is then a matter of swapping the active integrator; the scene and
/// presentation machinery stay put.
class Integrator {
public:
    virtual ~Integrator() = default;

    virtual const char* name() const = 0;

    /// @brief Recreates resolution-dependent resources.
    /// @warning The caller must ensure the GPU is idle.
    virtual void resize(vk::Extent2D extent) = 0;

    /// @brief Discards any accumulated state. Called when the camera or scene
    ///        changes; accumulating integrators restart from zero samples.
    virtual void reset() {}

    /// @brief Records this frame's work into FrameContext::command_buffer.
    virtual void record(const FrameContext& frame) = 0;

    /// @brief Linear HDR result of the last recorded frame.
    virtual const vulcao::ImageView& output_view() const = 0;
};

} // namespace vulchovy
