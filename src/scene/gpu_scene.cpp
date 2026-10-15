#include "scene/gpu_scene.h"

#include <algorithm>
#include <array>
#include <cmath>
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

/// @brief A node's primitive flattened to its world transform (what the TLAS and
///        the instance buffer consume).
struct DrawInstance {
    mat4 transform{1.0f};
    uint32_t mesh = 0;
    uint32_t material = 0;
};

std::vector<DrawInstance> flatten_nodes(const Scene& scene) {
    std::vector<DrawInstance> draw_instances;
    for (uint32_t node_index = 0; node_index < scene.nodes.size(); ++node_index) {
        const Node& node = scene.nodes[node_index];
        if (node.primitives.empty())
            continue;
        const mat4 world = node_world_transform(scene, node_index);
        for (const Primitive& primitive : node.primitives)
            draw_instances.push_back(DrawInstance{world, primitive.mesh, primitive.material});
    }
    return draw_instances;
}

float luminance(const vec3& color) {
    return 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
}

float srgb_to_linear(float c) {
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

vec3 sample_texture_linear(const TextureData& texture, const vec2& uv) {
    if (!texture.valid())
        return vec3{1.0f};
    const float u = uv.x - std::floor(uv.x);
    const float v = uv.y - std::floor(uv.y);
    const uint32_t x = std::min(static_cast<uint32_t>(u * texture.width), texture.width - 1);
    const uint32_t y = std::min(static_cast<uint32_t>(v * texture.height), texture.height - 1);
    const size_t base = (static_cast<size_t>(y) * texture.width + x) * 4;
    float channels[3];
    for (int c = 0; c < 3; ++c) {
        const float value = texture.pixels[base + c] / 255.0f;
        channels[c] = texture.srgb ? srgb_to_linear(value) : value;
    }
    return vec3{channels[0], channels[1], channels[2]};
}

/// @brief An emissive triangle plus its importance-sampling weight.
struct MeshLightBuild {
    std::vector<GpuMeshLight> triangles;
    std::vector<float> weights;
    std::vector<uint32_t> instance_base;
    float total_weight = 0.0f;
};

MeshLightBuild build_mesh_lights(const Scene& scene,
                                 const std::vector<DrawInstance>& draw_instances) {
    MeshLightBuild build;
    build.instance_base.assign(draw_instances.size(), 0xFFFFFFFFu);

    for (size_t di = 0; di < draw_instances.size(); ++di) {
        const DrawInstance& draw = draw_instances[di];
        const Material& material = scene.materials[draw.material];
        if (luminance(material.emission) <= 0.0f)
            continue;

        const Mesh& mesh = scene.meshes[draw.mesh];
        const uint32_t triangle_count = static_cast<uint32_t>(mesh.indices.size() / 3);
        if (triangle_count == 0)
            continue;

        build.instance_base[di] = static_cast<uint32_t>(build.triangles.size());
        for (uint32_t t = 0; t < triangle_count; ++t) {
            const Vertex& v0 = mesh.vertices[mesh.indices[t * 3 + 0]];
            const Vertex& v1 = mesh.vertices[mesh.indices[t * 3 + 1]];
            const Vertex& v2 = mesh.vertices[mesh.indices[t * 3 + 2]];
            const vec3 p0 = vec3(draw.transform * vec4{v0.position, 1.0f});
            const vec3 p1 = vec3(draw.transform * vec4{v1.position, 1.0f});
            const vec3 p2 = vec3(draw.transform * vec4{v2.position, 1.0f});

            const vec3 cross_product = cross(p1 - p0, p2 - p0);
            const float area = 0.5f * length(cross_product);
            const vec3 normal =
                area > 1e-12f ? cross_product / (2.0f * area) : vec3{0.0f, 1.0f, 0.0f};

            vec3 emission = material.emission;
            if (material.emissive_texture >= 0 &&
                static_cast<size_t>(material.emissive_texture) < scene.textures.size()) {
                const vec2 uv = (v0.uv + v1.uv + v2.uv) / 3.0f;
                emission *= sample_texture_linear(scene.textures[material.emissive_texture], uv);
            }

            build.triangles.push_back(GpuMeshLight{
                .p0 = vec4(p0, 0.0f),
                .p1 = vec4(p1, 0.0f),
                .p2 = vec4(p2, 0.0f),
                .normal = vec4(normal, area),
                .emission = vec4(emission, 0.0f),
            });
            const float weight = luminance(emission) * area;
            build.weights.push_back(weight);
            build.total_weight += weight;
        }
    }
    return build;
}

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
    if (vertices.empty())
        vertices.push_back(Vertex{});
    if (indices.empty())
        indices.push_back(0u);
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
            .textures = glm::ivec4(material.base_color_texture,
                                   material.metallic_roughness_texture,
                                   material.normal_texture, material.emissive_texture),
            .attenuation = vec4(material.attenuation, 0.0f),
        });
    }
    material_count_ = static_cast<uint32_t>(materials.size());
    if (materials.empty())
        materials.push_back(GpuMaterial{});
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
    if (lights.empty())
        lights.push_back(GpuLight{});
    lights_ = vulcao::Buffer::create_with_data(context, lights, kSceneBufferUsage);

    const std::vector<DrawInstance> draw_instances = flatten_nodes(scene);
    std::vector<GpuInstance> instances;
    instances.reserve(draw_instances.size());
    for (const DrawInstance& draw : draw_instances) {
        const MeshRange& range = ranges.at(draw.mesh);
        const mat4& to_world = draw.transform;
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
                                   draw.material),
        });
    }
    instance_count_ = static_cast<uint32_t>(instances.size());
    if (instances.empty())
        instances.push_back(GpuInstance{});
    instances_ = vulcao::Buffer::create_with_data(context, instances, kSceneBufferUsage);

    // Environment map: image, sampler and the importance-sampling CDFs. The
    // integrator needs these on the GPU before the scene descriptor set is built.
    environment_ = scene.environment;
    if (!environment_.valid())
        environment_ = make_black_environment();

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

    // Sampled texture array. Every slot is written (unused ones fall back to a
    // 1x1 white image) because a statically used descriptor array must be fully
    // populated before it is bound.
    texture_sampler_ = vulcao::Sampler::linear(context.device(), true);
    texture_images_.reserve(scene.textures.size());
    texture_views_.reserve(scene.textures.size());
    for (const TextureData& texture : scene.textures) {
        if (!texture.valid())
            continue;
        const vk::Format format =
            texture.srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
        vulcao::Image image = vulcao::Image::create_2d(
            context.allocator(), vk::Extent2D{texture.width, texture.height}, format,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
        context.upload(image, texture.pixels.data(), texture.pixels.size(),
                       vk::ImageLayout::eShaderReadOnlyOptimal, false);
        texture_views_.push_back(vulcao::ImageView::create(context.device(), image));
        texture_images_.push_back(std::move(image));
    }

    const std::array<uint8_t, 4> white{255, 255, 255, 255};
    fallback_texture_image_ = vulcao::Image::create_2d(
        context.allocator(), vk::Extent2D{1, 1}, vk::Format::eR8G8B8A8Unorm,
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
    context.upload(fallback_texture_image_, white.data(), white.size(),
                   vk::ImageLayout::eShaderReadOnlyOptimal, false);
    fallback_texture_view_ =
        vulcao::ImageView::create(context.device(), fallback_texture_image_);

    // Emissive-triangle area lights: geometry, an area-weighted CDF for sampling,
    // and a per-instance base index so a BSDF hit can recover its light.
    const MeshLightBuild mesh_lights = build_mesh_lights(scene, draw_instances);
    mesh_light_count_ = static_cast<uint32_t>(mesh_lights.triangles.size());
    mesh_light_total_ = mesh_lights.total_weight;

    std::vector<GpuMeshLight> light_triangles = mesh_lights.triangles;
    if (light_triangles.empty())
        light_triangles.push_back(GpuMeshLight{vec4{0.0f}, vec4{0.0f}, vec4{0.0f}, vec4{0.0f},
                                               vec4{0.0f}});

    std::vector<float> light_cdf(mesh_light_count_ + 1, 0.0f);
    if (mesh_light_count_ == 0) {
        light_cdf[0] = 1.0f;
    } else if (mesh_light_total_ > 0.0f) {
        float accum = 0.0f;
        for (uint32_t i = 0; i < mesh_light_count_; ++i) {
            accum += mesh_lights.weights[i];
            light_cdf[i + 1] = accum / mesh_light_total_;
        }
    } else {
        for (uint32_t i = 0; i < mesh_light_count_; ++i)
            light_cdf[i + 1] = static_cast<float>(i + 1) / static_cast<float>(mesh_light_count_);
    }

    std::vector<uint32_t> instance_base = mesh_lights.instance_base;
    if (instance_base.empty())
        instance_base.push_back(0xFFFFFFFFu);

    mesh_light_triangles_ =
        vulcao::Buffer::create_with_data(context, light_triangles, kSceneBufferUsage);
    mesh_light_cdf_ = vulcao::Buffer::create_with_data(context, light_cdf, kSceneBufferUsage);
    instance_light_base_ =
        vulcao::Buffer::create_with_data(context, instance_base, kSceneBufferUsage);

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

    // One top level instance per flattened node primitive.
    std::vector<vulcao::AccelerationStructureInstance> tlas_instances;
    tlas_instances.reserve(draw_instances.size());
    for (size_t i = 0; i < draw_instances.size(); ++i) {
        const DrawInstance& draw = draw_instances[i];
        const mat4& m = draw.transform;
        vulcao::AccelerationStructureInstance tlas_instance;
        tlas_instance.acceleration_structure = blas_[draw.mesh].handle();
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
