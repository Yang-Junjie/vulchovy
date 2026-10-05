#include "scene/gpu_scene.h"

#include <algorithm>
#include <span>

#include <glm/gtc/matrix_inverse.hpp>

#include <vulcao/command_buffer.h>
#include <vulcao/context.h>

namespace vulchovy {

namespace {

vk::BufferUsageFlags geometry_usage(bool build_acceleration_structures) {
    vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eStorageBuffer;
    if (build_acceleration_structures) {
        usage |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress;
    }
    return usage;
}

constexpr vk::BufferUsageFlags kSceneBufferUsage = vk::BufferUsageFlagBits::eStorageBuffer;

} // namespace

GpuScene::GpuScene(vulcao::Context& context, const Scene& scene, bool build_acceleration_structures) {
    // Merge the meshes into one vertex and one index buffer.
    std::vector<MeshRange> ranges;
    ranges.reserve(scene.meshes.size());
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    for (const Mesh& mesh : scene.meshes) {
        MeshRange range;
        range.vertex_offset = static_cast<uint32_t>(vertices.size());
        range.index_offset = static_cast<uint32_t>(indices.size());
        range.index_count = static_cast<uint32_t>(mesh.indices.size());
        vertices.insert(vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
        indices.insert(indices.end(), mesh.indices.begin(), mesh.indices.end());
        ranges.push_back(range);
    }

    const vk::BufferUsageFlags usage = geometry_usage(build_acceleration_structures);
    vertices_ = vulcao::Buffer::create_with_data(context, vertices, usage);
    indices_ = vulcao::Buffer::create_with_data(context, indices, usage);

    std::vector<GpuMaterial> materials;
    materials.reserve(scene.materials.size());
    for (const Material& material : scene.materials) {
        materials.push_back(GpuMaterial{
            .base_color = vec4(material.base_color, 0.0f),
            .emission = vec4(material.emission, 0.0f),
            .params = vec4(material.metallic, material.roughness, material.transmission,
                           material.ior),
            .params2 = vec4(material.anisotropy, 0.0f, 0.0f, 0.0f),
        });
    }
    material_count_ = static_cast<uint32_t>(materials.size());
    materials_ = vulcao::Buffer::create_with_data(context, materials, kSceneBufferUsage);

    std::vector<GpuLight> lights;
    lights.reserve(scene.lights.size());
    for (const Light& light : scene.lights) {
        lights.push_back(GpuLight{
            .position = vec4(light.position, 0.0f),
            .emission = vec4(light.emission, 0.0f),
            .params = vec4(light.radius, 0.0f, 0.0f, 0.0f),
        });
    }
    light_count_ = static_cast<uint32_t>(lights.size());
    lights_ = vulcao::Buffer::create_with_data(context, lights, kSceneBufferUsage);

    std::vector<GpuInstance> instances;
    instances.reserve(scene.instances.size());
    for (const Instance& instance : scene.instances) {
        const MeshRange& range = ranges.at(instance.mesh_index);
        const mat4& to_world = instance.transform;
        const mat4 to_object = glm::inverse(to_world);
        instances.push_back(GpuInstance{
            .to_world_x = to_world[0],
            .to_world_y = to_world[1],
            .to_world_z = to_world[2],
            .to_world_t = to_world[3],
            .to_object_x = to_object[0],
            .to_object_y = to_object[1],
            .to_object_z = to_object[2],
            .to_object_t = to_object[3],
            .geometry = glm::uvec4(range.vertex_offset, range.index_offset, range.index_count,
                                   instance.material_index),
        });
    }
    instance_count_ = static_cast<uint32_t>(instances.size());
    instances_ = vulcao::Buffer::create_with_data(context, instances, kSceneBufferUsage);

    // Environment map: image, sampler and the importance-sampling CDFs. The
    // integrator needs these on the GPU before the scene descriptor set is built.
    environment_ = scene.environment;
    if (!environment_.valid())
        environment_ = make_procedural_sky();

    const EnvironmentSampling sampling = build_environment_sampling(environment_);
    env_pdf_scale_ = sampling.pdf_scale;
    std::vector<float> marginal = sampling.marginal;
    std::vector<float> conditional = sampling.conditional;
    if (marginal.empty())
        marginal.push_back(1.0f);
    if (conditional.empty())
        conditional.push_back(1.0f);
    env_marginal_ = vulcao::Buffer::create_with_data(context, marginal, kSceneBufferUsage);
    env_conditional_ = vulcao::Buffer::create_with_data(context, conditional, kSceneBufferUsage);

    uint32_t mip_levels = 1;
    while ((1u << mip_levels) < std::max(environment_.width, environment_.height))
        ++mip_levels;

    environment_image_ = vulcao::Image::create_2d(
        context.allocator(), vk::Extent2D{environment_.width, environment_.height},
        vk::Format::eR32G32B32A32Sfloat,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eTransferSrc,
        mip_levels);
    environment_view_ = vulcao::ImageView::create(context.device(), environment_image_);
    environment_sampler_ = vulcao::Sampler::create(
        context.device(),
        vk::SamplerCreateInfo{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eClampToEdge,
            .addressModeW = vk::SamplerAddressMode::eClampToEdge,
            .maxLod = static_cast<float>(mip_levels),
        });

    std::vector<vec4> texels(environment_.pixels.size());
    for (size_t i = 0; i < texels.size(); ++i)
        texels[i] = vec4(environment_.pixels[i], 1.0f);
    context.upload(environment_image_, texels.data(), texels.size() * sizeof(vec4),
                   vk::ImageLayout::eShaderReadOnlyOptimal, true);

    scene_descriptors_ = SceneDescriptors(context.device(), *this);

    if (!build_acceleration_structures)
        return;

    // One bottom level structure per mesh.
    std::vector<vulcao::TrianglesGeometry> geometries(scene.meshes.size());
    blas_.reserve(scene.meshes.size());
    for (size_t mesh_index = 0; mesh_index < scene.meshes.size(); ++mesh_index) {
        const MeshRange& range = ranges[mesh_index];
        geometries[mesh_index] = vulcao::TrianglesGeometry{
            .vertex_data = vertices_.device_address() + range.vertex_offset * sizeof(Vertex),
            .index_data = indices_.device_address() + range.index_offset * sizeof(uint32_t),
            .vertex_count = static_cast<uint32_t>(scene.meshes[mesh_index].vertices.size()),
            .index_count = range.index_count,
            .vertex_stride = sizeof(Vertex),
            .vertex_format = vk::Format::eR32G32B32Sfloat,
            .index_type = vk::IndexType::eUint32,
            .flags = vk::GeometryFlagBitsKHR::eOpaque,
        };
        blas_.push_back(vulcao::AccelerationStructure::create_blas(
            context, std::span(&geometries[mesh_index], 1), {}, true));
    }
    context.immediate([&](vulcao::CommandBuffer& cmd) {
        for (size_t mesh_index = 0; mesh_index < blas_.size(); ++mesh_index)
            cmd.build_acceleration_structure(blas_[mesh_index],
                                             std::span(&geometries[mesh_index], 1));
    });

    // One top level instance per scene object.
    std::vector<vulcao::AccelerationStructureInstance> tlas_instances;
    tlas_instances.reserve(scene.instances.size());
    for (size_t i = 0; i < scene.instances.size(); ++i) {
        const Instance& instance = scene.instances[i];
        const mat4& m = instance.transform;
        vulcao::AccelerationStructureInstance tlas_instance;
        tlas_instance.acceleration_structure = blas_[instance.mesh_index].handle();
        tlas_instance.instance_custom_index = static_cast<uint32_t>(i);
        tlas_instance.mask = 0xFF;
        tlas_instance.transform = {m[0][0], m[1][0], m[2][0], m[3][0],
                                   m[0][1], m[1][1], m[2][1], m[3][1],
                                   m[0][2], m[1][2], m[2][2], m[3][2]};
        tlas_instances.push_back(tlas_instance);
    }
    instance_buffer_ = vulcao::make_instance_buffer(context, tlas_instances);
    tlas_ = vulcao::AccelerationStructure::create_tlas(
        context, static_cast<uint32_t>(tlas_instances.size()), {}, true);
    context.immediate([&](vulcao::CommandBuffer& cmd) {
        cmd.build_acceleration_structure(tlas_, instance_buffer_);
    });
}

} // namespace vulchovy
