#pragma once

#include "render/integrators/ray_query/ray_query_integrator.h"

namespace vulchovy::ray_query {

/// @brief Debug integrator that shades the world-space normal of the closest
///        hit. Exercises the shared ray query base with a second, smaller kernel.
class DebugNormals : public RayQueryIntegrator {
public:
    DebugNormals(vulcao::Context& context, const GpuScene& scene);

    const char* name() const override { return "Normals"; }
    void record(const FrameContext& frame) override;
};

} // namespace vulchovy::ray_query
