// Debug integrator: shades the world-space normal of the closest hit.

#include "common/scene.slang"

struct CameraData {
    float4 position;
    float4 forward;
    float4 right;
    float4 up;
};

struct Uniforms {
    CameraData camera;
    uint4 options; // x = width, y = height, z = frame
};

[[vk::binding(0, 1)]] ConstantBuffer<Uniforms> g_uniforms;
[[vk::binding(1, 1)]] RWTexture2D<float4> g_output;
[[vk::binding(2, 1)]] RaytracingAccelerationStructure g_tlas;

float3 background(float3 direction) {
    const float t = saturate(0.5 * (direction.y + 1.0));
    return lerp(float3(0.05, 0.05, 0.08), float3(0.2, 0.2, 0.25), t);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread_id : SV_DispatchThreadID) {
    const uint width = g_uniforms.options.x;
    const uint height = g_uniforms.options.y;
    const uint2 pixel = thread_id.xy;
    if (pixel.x >= width || pixel.y >= height)
        return;

    const float2 uv = (float2(pixel) + 0.5) / float2(width, height);
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    const CameraData camera = g_uniforms.camera;
    const float3 origin = camera.position.xyz;
    const float3 direction = normalize(camera.forward.xyz + camera.right.xyz * ndc.x +
                                       camera.up.xyz * ndc.y);

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.001;
    ray.TMax = 1e30;

    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(g_tlas, RAY_FLAG_NONE, 0xFF, ray);
    while (query.Proceed()) {
    }

    float3 color = background(direction);
    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        const GpuInstance instance = g_instances[query.CommittedInstanceIndex()];
        const uint primitive = query.CommittedPrimitiveIndex();
        const float2 bary = query.CommittedTriangleBarycentrics();
        const float3 weights = float3(1.0 - bary.x - bary.y, bary.x, bary.y);

        const uint base = instance.geometry.y + primitive * 3;
        const Vertex v0 = g_vertices[instance.geometry.x + g_indices[base + 0]];
        const Vertex v1 = g_vertices[instance.geometry.x + g_indices[base + 1]];
        const Vertex v2 = g_vertices[instance.geometry.x + g_indices[base + 2]];

        const float3 object_normal = weights.x * v0.normal + weights.y * v1.normal +
                                     weights.z * v2.normal;
        const float3 normal = normalize(instance_to_world_dir(instance, object_normal));
        color = normal * 0.5 + 0.5;
    }

    g_output[pixel] = float4(color, 1.0);
}
