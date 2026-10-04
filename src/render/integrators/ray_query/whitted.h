#pragma once

#include "render/integrators/ray_query/ray_query_integrator.h"

namespace vulchovy::ray_query {

/// @brief Whitted-style ray tracing: ambient, hard shadows and recursive
///        specular reflections, built on inline ray queries.
class Whitted : public RayQueryIntegrator {
public:
    Whitted(vulcao::Context& context, const GpuScene& scene);

    const char* name() const override { return "Whitted"; }
    void record(const FrameContext& frame) override;

private:
    uint32_t max_depth_ = 4;
};

} // namespace vulchovy::ray_query
