#pragma once

#include "render/integrators/ray_query/ray_query_integrator.h"

namespace vulchovy::ray_query {

/// @brief Unidirectional path tracer with next-event estimation and MIS.
///
/// Accumulates samples across frames (running average) so a static camera
/// converges; reset() restarts from zero samples.
class PathTracer : public RayQueryIntegrator {
public:
    PathTracer(vulcao::Context& context, const GpuScene& scene);

    const char* name() const override { return "PathTracer"; }
    void reset() override;
    void record(const FrameContext& frame) override;

private:
    uint32_t max_depth_ = 8;
    uint32_t sample_count_ = 0;
    bool needs_clear_ = true;
};

} // namespace vulchovy::ray_query
