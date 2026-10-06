#pragma once

// Scene data shared by every integrator, bound at set 0.
//
// Mirrors src/scene/gpu_types.h. The instance transforms are stored as explicit
// basis vectors to avoid depending on matrix memory layout conventions.

static const float kPi = 3.14159265358979;
static const uint kMaxSceneTextures = 64;

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
};

struct GpuMaterial {
    float4 base_color;
    float4 emission;
    float4 params;  // x = metallic, y = roughness, z = transmission, w = ior
    float4 params2; // x = anisotropy
    int4 textures;  // x = base color, y = metal-rough, z = normal, w = emissive
    float4 attenuation; // rgb = volume attenuation colour
};

struct GpuLight {
    float4 position; // xyz = world position
    float4 emission; // rgb = emitted radiance / intensity
    float4 params;   // x = radius (0 = point light)
};

struct GpuMeshLight {
    float4 p0;       // xyz = world position
    float4 p1;
    float4 p2;
    float4 normal;   // xyz = geometric normal, w = area
    float4 emission; // rgb = emitted radiance (linear)
};

struct GpuInstance {
    float4 to_world_x;
    float4 to_world_y;
    float4 to_world_z;
    float4 to_world_t;
    float4 to_object_x;
    float4 to_object_y;
    float4 to_object_z;
    float4 to_object_t;
    uint4 geometry; // x = vertex offset, y = index offset, z = index count, w = material index
};

[[vk::binding(0, 0)]] StructuredBuffer<GpuMaterial> g_materials;
[[vk::binding(1, 0)]] StructuredBuffer<GpuLight> g_lights;
[[vk::binding(2, 0)]] StructuredBuffer<uint> g_indices;
[[vk::binding(3, 0)]] StructuredBuffer<Vertex> g_vertices;
[[vk::binding(4, 0)]] StructuredBuffer<GpuInstance> g_instances;
[[vk::binding(9, 0)]] StructuredBuffer<GpuMeshLight> g_mesh_lights;
[[vk::binding(10, 0)]] StructuredBuffer<float> g_mesh_light_cdf;
[[vk::binding(11, 0)]] StructuredBuffer<uint> g_instance_light_base;

// Sampled texture array. Colour maps use an sRGB image format, so the hardware
// decodes them to linear on sample; data maps use a linear format.
[[vk::binding(8, 0)]] Sampler2D g_textures[kMaxSceneTextures];

/// @brief Samples a texture slot, returning white when the slot is unused.
float3 sample_texture_or_white(int index, float2 uv) {
    if (index < 0)
        return float3(1.0, 1.0, 1.0);
    return g_textures[index].SampleLevel(uv, 0.0).rgb;
}

float3 instance_to_world_point(GpuInstance instance, float3 p) {
    return instance.to_world_x.xyz * p.x + instance.to_world_y.xyz * p.y +
           instance.to_world_z.xyz * p.z + instance.to_world_t.xyz;
}

float3 instance_to_world_dir(GpuInstance instance, float3 d) {
    return instance.to_world_x.xyz * d.x + instance.to_world_y.xyz * d.y +
           instance.to_world_z.xyz * d.z;
}

float3 instance_to_object_point(GpuInstance instance, float3 p) {
    return instance.to_object_x.xyz * p.x + instance.to_object_y.xyz * p.y +
           instance.to_object_z.xyz * p.z + instance.to_object_t.xyz;
}

float3 instance_to_object_dir(GpuInstance instance, float3 d) {
    return instance.to_object_x.xyz * d.x + instance.to_object_y.xyz * d.y +
           instance.to_object_z.xyz * d.z;
}

/// @brief Offsets a ray origin along the shading normal so a secondary ray does
///        not immediately re-hit the surface it started on. The epsilon scales
///        with the position magnitude to stay effective at grazing angles.
float3 offset_ray_origin(float3 position, float3 normal, float3 direction) {
    float3 offset = normal * (1e-3 * (1.0 + length(position)));
    if (dot(direction, normal) < 0.0)
        offset = -offset;
    return position + offset;
}
