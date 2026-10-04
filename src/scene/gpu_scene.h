#pragma once

#include <cstdint>
#include <vector>

#include <vulcao/acceleration_structure.h>
#include <vulcao/buffer.h>

#include "scene/gpu_types.h"
#include "scene/scene.h"
#include "scene/scene_descriptors.h"

namespace vulcao {
class Context;
} // namespace vulcao

namespace vulchovy {

/// @brief GPU residency of a CPU scene: merged geometry, per-object material,
///        light and instance buffers, the shared scene descriptor set and,
///        optionally, the acceleration structures a ray query kernel traces.
///
/// Geometry is merged into a single vertex and index buffer; each instance
/// stores the offsets into them. When @p build_acceleration_structures is set,
/// one bottom level structure is built per mesh and a single top level structure
/// holds one instance per scene object; a pure compute integrator can skip them
/// and run without the ray query feature.
class GpuScene {
public:
    GpuScene(vulcao::Context& context,
             const Scene& scene,
             bool build_acceleration_structures = true);

    GpuScene(const GpuScene&) = delete;
    GpuScene& operator=(const GpuScene&) = delete;

    const vulcao::Buffer& vertices() const { return vertices_; }
    const vulcao::Buffer& indices() const { return indices_; }
    const vulcao::Buffer& materials() const { return materials_; }
    const vulcao::Buffer& lights() const { return lights_; }
    const vulcao::Buffer& instances() const { return instances_; }
    const vulcao::AccelerationStructure& tlas() const { return tlas_; }
    const SceneDescriptors& scene_descriptors() const { return scene_descriptors_; }

    uint32_t material_count() const { return material_count_; }
    uint32_t light_count() const { return light_count_; }
    uint32_t instance_count() const { return instance_count_; }

private:
    struct MeshRange {
        uint32_t vertex_offset = 0;
        uint32_t index_offset = 0;
        uint32_t index_count = 0;
    };

    vulcao::Buffer vertices_;
    vulcao::Buffer indices_;
    vulcao::Buffer materials_;
    vulcao::Buffer lights_;
    vulcao::Buffer instances_;

    std::vector<vulcao::AccelerationStructure> blas_;
    vulcao::Buffer instance_buffer_;
    vulcao::AccelerationStructure tlas_;

    SceneDescriptors scene_descriptors_;

    uint32_t material_count_ = 0;
    uint32_t light_count_ = 0;
    uint32_t instance_count_ = 0;
};

} // namespace vulchovy
