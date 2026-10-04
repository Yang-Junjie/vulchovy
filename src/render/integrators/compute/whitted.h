#pragma once

#include "render/integrators/compute/compute_integrator.h"

namespace vulcao {
class Context;
} // namespace vulcao

namespace vulchovy {

class GpuScene;

namespace compute {

/// @brief Whitted-style ray tracing in a pure compute shader.
///
/// Traverses every triangle in a brute-force loop instead of using hardware ray
/// queries, so it runs on devices without the ray query extension. The shading
/// matches the ray query Whitted integrator; only the visibility test differs.
class Whitted : public ComputeIntegrator {
public:
    Whitted(vulcao::Context& context, const GpuScene& scene);

    const char* name() const override { return "WhittedCompute"; }
    void record(const FrameContext& frame) override;

private:
    const GpuScene& scene_;
    uint32_t max_depth_ = 4;
};

} // namespace compute
} // namespace vulchovy
