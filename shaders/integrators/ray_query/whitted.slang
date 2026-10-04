// Whitted-style ray tracing built on inline ray queries.
//
// Scene data (materials, lights, geometry, instances) comes from the shared set
// 0 declared in common/scene.slang; the uniform block, output image and top
// level acceleration structure are this integrator's own set 1.

#include "common/scene.slang"

struct CameraData {
    float4 position;
    float4 forward;
    float4 right;
    float4 up;
};

struct Uniforms {
    CameraData camera;
    uint4 counts;  // x = material count, y = light count, z = instance count, w = frame
    uint4 options; // x = max depth, y = width, z = height
};

[[vk::binding(0, 1)]] ConstantBuffer<Uniforms> g_uniforms;
[[vk::binding(1, 1)]] RWTexture2D<float4> g_output;
[[vk::binding(2, 1)]] RaytracingAccelerationStructure g_tlas;

struct HitInfo {
    float3 position;
    float3 normal;
    float3 albedo;
    float3 specular;
    float shininess;
    float reflectivity;
};

bool trace_scene(float3 origin, float3 direction, float t_min, float t_max, out HitInfo hit) {
    hit = {};

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = t_min;
    ray.TMax = t_max;

    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(g_tlas, RAY_FLAG_NONE, 0xFF, ray);
    while (query.Proceed()) {
    }

    if (query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
        return false;

    const GpuInstance instance = g_instances[query.CommittedInstanceIndex()];
    const uint primitive = query.CommittedPrimitiveIndex();
    const float2 bary = query.CommittedTriangleBarycentrics();
    const float3 weights = float3(1.0 - bary.x - bary.y, bary.x, bary.y);

    const uint base = instance.geometry.y + primitive * 3;
    const uint i0 = g_indices[base + 0];
    const uint i1 = g_indices[base + 1];
    const uint i2 = g_indices[base + 2];
    const Vertex v0 = g_vertices[instance.geometry.x + i0];
    const Vertex v1 = g_vertices[instance.geometry.x + i1];
    const Vertex v2 = g_vertices[instance.geometry.x + i2];

    const float3 object_normal = weights.x * v0.normal + weights.y * v1.normal +
                                 weights.z * v2.normal;
    float3 normal = normalize(instance_to_world_dir(instance, object_normal));

    // Keep shading two-sided so the same kernel works from inside a mesh.
    if (dot(normal, direction) > 0.0)
        normal = -normal;

    const GpuMaterial material = g_materials[instance.geometry.w];

    // Deprecated approximation of the PBR material.
    const float metallic = material.params.x;
    const float roughness = max(material.params.y, 0.04);
    const float shininess = max(2.0 / (roughness * roughness) - 2.0, 1.0);

    hit.position = origin + direction * query.CommittedRayT();
    hit.normal = normal;
    hit.albedo = material.base_color.rgb * (1.0 - metallic);
    hit.specular = lerp(float3(0.04, 0.04, 0.04), material.base_color.rgb, metallic);
    hit.shininess = shininess;
    hit.reflectivity = metallic;
    return true;
}

bool occluded(float3 origin, float3 direction, float t_max) {
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.001;
    ray.TMax = t_max;

    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> query;
    query.TraceRayInline(g_tlas, RAY_FLAG_NONE, 0xFF, ray);
    while (query.Proceed()) {
    }
    return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

float3 background(float3 direction) {
    const float t = saturate(0.5 * (direction.y + 1.0));
    return lerp(float3(0.02, 0.03, 0.05), float3(0.18, 0.24, 0.35), t);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread_id : SV_DispatchThreadID) {
    const uint width = g_uniforms.options.y;
    const uint height = g_uniforms.options.z;
    const uint2 pixel = thread_id.xy;
    if (pixel.x >= width || pixel.y >= height)
        return;

    const float2 uv = (float2(pixel) + 0.5) / float2(width, height);
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    const CameraData camera = g_uniforms.camera;
    float3 origin = camera.position.xyz;
    float3 direction = normalize(camera.forward.xyz + camera.right.xyz * ndc.x +
                                 camera.up.xyz * ndc.y);

    float3 color = float3(0.0, 0.0, 0.0);
    float3 throughput = float3(1.0, 1.0, 1.0);
    const uint max_depth = g_uniforms.options.x;
    const uint light_count = g_uniforms.counts.y;

    for (uint depth = 0; depth <= max_depth; ++depth) {
        HitInfo hit;
        if (!trace_scene(origin, direction, 0.001, 1e30, hit)) {
            color += throughput * background(direction);
            break;
        }

        color += throughput * 0.03 * hit.albedo;

        for (uint light_index = 0; light_index < light_count; ++light_index) {
            const GpuLight light = g_lights[light_index];
            const float3 to_light = light.position.xyz - hit.position;
            const float distance = length(to_light);
            const float3 light_dir = to_light / distance;
            if (occluded(hit.position + hit.normal * 0.001, light_dir, distance - 0.001))
                continue;

            const float n_dot_l = max(dot(hit.normal, light_dir), 0.0);
            const float3 radiance = light.emission.rgb * n_dot_l / (distance * distance);
            color += throughput * hit.albedo * radiance;

            const float3 view = -direction;
            const float3 half_vector = normalize(light_dir + view);
            const float specular = pow(max(dot(hit.normal, half_vector), 0.0), hit.shininess);
            color += throughput * hit.specular * radiance * specular;
        }

        throughput *= hit.reflectivity;
        if (max(throughput.r, max(throughput.g, throughput.b)) < 0.001)
            break;

        direction = reflect(direction, hit.normal);
        origin = hit.position + hit.normal * 0.001;
    }

    g_output[pixel] = float4(color, 1.0);
}
