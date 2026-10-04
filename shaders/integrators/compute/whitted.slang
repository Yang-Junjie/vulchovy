// Whitted-style ray tracing in a pure compute shader.
//
// Software ray tracing: for every instance the ray is transformed to object
// space and every triangle of its mesh is tested in a brute-force loop, so no
// acceleration structure or ray query extension is needed. Scene data comes
// from the shared set 0; the uniform block and output image are set 1.

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

struct HitInfo {
    float3 position;
    float3 normal;
    float3 albedo;
    float3 specular;
    float shininess;
    float reflectivity;
};

// Moller-Trumbore triangle intersection, back-face culling disabled.
bool intersect_triangle(Vertex v0,
                        Vertex v1,
                        Vertex v2,
                        float3 origin,
                        float3 direction,
                        float t_min,
                        float t_max,
                        out float hit_t,
                        out float3 hit_normal) {
    hit_t = 0.0;
    hit_normal = float3(0.0, 0.0, 0.0);

    const float3 edge1 = v1.position - v0.position;
    const float3 edge2 = v2.position - v0.position;
    const float3 pvec = cross(direction, edge2);
    const float determinant = dot(edge1, pvec);
    if (abs(determinant) < 1e-8)
        return false;

    const float inv_determinant = 1.0 / determinant;
    const float3 tvec = origin - v0.position;
    const float u = dot(tvec, pvec) * inv_determinant;
    if (u < 0.0 || u > 1.0)
        return false;

    const float3 qvec = cross(tvec, edge1);
    const float v = dot(direction, qvec) * inv_determinant;
    if (v < 0.0 || u + v > 1.0)
        return false;

    const float t = dot(edge2, qvec) * inv_determinant;
    if (t < t_min || t > t_max)
        return false;

    hit_t = t;
    hit_normal = (1.0 - u - v) * v0.normal + u * v1.normal + v * v2.normal;
    return true;
}

bool trace_scene(float3 origin, float3 direction, float t_min, float t_max, out HitInfo hit) {
    hit = {};

    float closest = t_max;
    bool found = false;
    float3 best_position = float3(0.0, 0.0, 0.0);
    float3 best_normal = float3(0.0, 0.0, 0.0);
    uint best_material = 0;

    const uint instance_count = g_uniforms.counts.z;
    for (uint instance_index = 0; instance_index < instance_count; ++instance_index) {
        const GpuInstance instance = g_instances[instance_index];
        const float3 object_origin = instance_to_object_point(instance, origin);
        const float3 object_dir = instance_to_object_dir(instance, direction);

        const uint begin = instance.geometry.y;
        const uint end = begin + instance.geometry.z;
        for (uint index = begin; index + 2 < end; index += 3) {
            const Vertex v0 = g_vertices[instance.geometry.x + g_indices[index + 0]];
            const Vertex v1 = g_vertices[instance.geometry.x + g_indices[index + 1]];
            const Vertex v2 = g_vertices[instance.geometry.x + g_indices[index + 2]];

            float object_t;
            float3 object_normal;
            if (!intersect_triangle(v0, v1, v2, object_origin, object_dir, 0.0, 1e30, object_t,
                                    object_normal))
                continue;

            const float3 object_position = object_origin + object_dir * object_t;
            const float3 world_position = instance_to_world_point(instance, object_position);
            const float world_t = length(world_position - origin);
            if (world_t < t_min || world_t >= closest)
                continue;

            found = true;
            closest = world_t;
            best_position = world_position;
            best_normal = normalize(instance_to_world_dir(instance, object_normal));
            best_material = instance.geometry.w;
        }
    }

    if (!found)
        return false;

    float3 normal = best_normal;
    if (dot(normal, direction) > 0.0)
        normal = -normal;

    const GpuMaterial material = g_materials[best_material];

    // Deprecated approximation of the PBR material.
    const float metallic = material.params.x;
    const float roughness = max(material.params.y, 0.04);
    const float shininess = max(2.0 / (roughness * roughness) - 2.0, 1.0);

    hit.position = best_position;
    hit.normal = normal;
    hit.albedo = material.base_color.rgb * (1.0 - metallic);
    hit.specular = lerp(float3(0.04, 0.04, 0.04), material.base_color.rgb, metallic);
    hit.shininess = shininess;
    hit.reflectivity = metallic;
    return true;
}

bool occluded(float3 origin, float3 direction, float t_max) {
    const uint instance_count = g_uniforms.counts.z;
    for (uint instance_index = 0; instance_index < instance_count; ++instance_index) {
        const GpuInstance instance = g_instances[instance_index];
        const float3 object_origin = instance_to_object_point(instance, origin);
        const float3 object_dir = instance_to_object_dir(instance, direction);

        const uint begin = instance.geometry.y;
        const uint end = begin + instance.geometry.z;
        for (uint index = begin; index + 2 < end; index += 3) {
            const Vertex v0 = g_vertices[instance.geometry.x + g_indices[index + 0]];
            const Vertex v1 = g_vertices[instance.geometry.x + g_indices[index + 1]];
            const Vertex v2 = g_vertices[instance.geometry.x + g_indices[index + 2]];

            float object_t;
            float3 object_normal;
            if (!intersect_triangle(v0, v1, v2, object_origin, object_dir, 0.0, 1e30, object_t,
                                    object_normal))
                continue;

            const float3 object_position = object_origin + object_dir * object_t;
            const float3 world_position = instance_to_world_point(instance, object_position);
            const float world_t = length(world_position - origin);
            if (world_t > 0.001 && world_t < t_max)
                return true;
        }
    }
    return false;
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
