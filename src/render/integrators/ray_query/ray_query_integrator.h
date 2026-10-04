#pragma once

#include <cstddef>
#include <filesystem>

#include "render/integrators/compute/compute_integrator.h"

namespace vulcao {
class Context;
} // namespace vulcao

namespace vulchovy {

class GpuScene;

namespace ray_query {

/// @brief Base for integrators implemented as a ray query compute kernel over
///        the scene acceleration structure.
///
/// Adds the top level acceleration structure binding (b0) and the geometry
/// buffers (b3 indices, b4 vertices) on top of ComputeIntegrator. Concrete
/// integrators only provide a shader and implement record().
class RayQueryIntegrator : public compute::ComputeIntegrator {
public:
    RayQueryIntegrator(vulcao::Context& context,
                       const GpuScene& scene,
                       const std::filesystem::path& shader_path,
                       const char* entry,
                       size_t uniform_bytes);

protected:
    const GpuScene& scene_;
};

} // namespace ray_query
} // namespace vulchovy
