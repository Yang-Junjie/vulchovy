// Unidirectional path tracer with next-event estimation and MIS, built on
// inline ray queries and analytic sphere area lights (point lights are delta).
//
// The surface response is a metallic-roughness PBR BSDF (Lambertian + GGX,
// see common/bsdf.slang). Samples are accumulated into the output image across
// frames (running average); the application calls reset() when the camera moves.

#include "common/scene.slang"
#include "common/bsdf.slang"
#include "common/random.slang"

struct CameraData {
    float4 position;
    float4 forward;
    float4 right;
    float4 up;
};

struct Uniforms {
    CameraData camera;
    uint4 counts;  // x = material count, y = light count, z = instance count, w = frame
    uint4 options; // x = max depth, y = width, z = height, w = sample count
};

[[vk::binding(0, 1)]] ConstantBuffer<Uniforms> g_uniforms;
[[vk::binding(1, 1)]] RWTexture2D<float4> g_accumulation;
[[vk::binding(2, 1)]] RaytracingAccelerationStructure g_tlas;

static const uint kNoLight = 0xFFFFFFFFu;
static const float kShadowEpsilon = 1e-3;

struct HitInfo {
    float3 position;
    float3 normal;
    float3 emission;
    GpuMaterial material;
    uint light_index; // kNoLight when the hit is not a light
};

bool intersect_sphere(float3 center,
                      float radius,
                      float3 origin,
                      float3 direction,
                      float t_min,
                      float t_max,
                      out float t) {
    t = 0.0;
    const float3 oc = origin - center;
    const float a = dot(direction, direction);
    const float b = 2.0 * dot(oc, direction);
    const float c = dot(oc, oc) - radius * radius;
    const float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0)
        return false;

    const float sq = sqrt(discriminant);
    const float t0 = (-b - sq) / (2.0 * a);
    const float t1 = (-b + sq) / (2.0 * a);
    t = t0;
    if (t < t_min)
        t = t1;
    return t >= t_min && t <= t_max;
}

bool trace_scene(float3 origin, float3 direction, float t_min, float t_max, out HitInfo hit) {
    hit = {};
    hit.light_index = kNoLight;

    bool found = false;
    float closest = t_max;
    float3 best_position = float3(0.0, 0.0, 0.0);
    float3 best_normal = float3(0.0, 0.0, 0.0);
    uint best_material = 0;

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = t_min;
    ray.TMax = t_max;

    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(g_tlas, RAY_FLAG_NONE, 0xFF, ray);
    while (query.Proceed()) {
    }

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
        float3 normal = normalize(instance_to_world_dir(instance, object_normal));
        if (dot(normal, direction) > 0.0)
            normal = -normal;

        found = true;
        closest = query.CommittedRayT();
        best_position = origin + direction * closest;
        best_normal = normal;
        best_material = instance.geometry.w;
    }

    // Analytic sphere lights are not in the acceleration structure.
    const uint light_count = g_uniforms.counts.y;
    uint best_light = kNoLight;
    for (uint i = 0; i < light_count; ++i) {
        const GpuLight light = g_lights[i];
        const float radius = light.params.x;
        if (radius <= 0.0)
            continue;
        float t;
        if (intersect_sphere(light.position.xyz, radius, origin, direction, t_min, closest, t)) {
            found = true;
            closest = t;
            best_position = origin + direction * t;
            best_normal = normalize(best_position - light.position.xyz);
            best_light = i;
        }
    }

    if (!found)
        return false;

    hit.position = best_position;
    hit.normal = best_normal;
    hit.light_index = best_light;
    if (best_light != kNoLight) {
        hit.emission = g_lights[best_light].emission.rgb;
    } else {
        hit.material = g_materials[best_material];
        hit.emission = hit.material.emission.rgb;
    }
    return true;
}

bool occluded(float3 origin, float3 direction, float t_max) {
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = kShadowEpsilon;
    ray.TMax = t_max;

    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> query;
    query.TraceRayInline(g_tlas, RAY_FLAG_NONE, 0xFF, ray);
    while (query.Proceed()) {
    }
    return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

// Uniform-cone sampling of a sphere light seen from p (PBRT-style spherical
// light sampling). Returns the direction, the distance to the sphere and the
// solid-angle pdf.
float3 sample_sphere_light(float3 p,
                           float3 center,
                           float radius,
                           float2 u,
                           out float distance_to_surface,
                           out float pdf) {
    const float3 to_center = center - p;
    const float dc2 = dot(to_center, to_center);
    const float dc = sqrt(dc2);
    const float3 w = to_center / dc;

    const float sin_theta_max2 = min(1.0, radius * radius / dc2);
    const float cos_theta_max = sqrt(max(0.0, 1.0 - sin_theta_max2));

    const float cos_theta = 1.0 - u.x * (1.0 - cos_theta_max);
    const float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
    const float phi = 2.0 * kPi * u.y;

    float3 tangent;
    float3 bitangent;
    make_basis(w, tangent, bitangent);
    const float3 direction = normalize(tangent * (sin_theta * cos(phi)) +
                                      bitangent * (sin_theta * sin(phi)) + w * cos_theta);

    distance_to_surface =
        dc * cos_theta - sqrt(max(0.0, radius * radius - dc2 * sin_theta * sin_theta));
    pdf = 1.0 / (2.0 * kPi * max(1e-6, 1.0 - cos_theta_max));
    return direction;
}

float sphere_light_pdf(float3 p, float3 center, float radius, float3 direction) {
    const float3 to_center = center - p;
    const float dc2 = dot(to_center, to_center);
    const float dc = sqrt(dc2);
    const float sin_theta_max2 = min(1.0, radius * radius / dc2);
    const float cos_theta_max = sqrt(max(0.0, 1.0 - sin_theta_max2));
    const float cos_theta = dot(to_center / dc, direction);
    if (cos_theta < cos_theta_max)
        return 0.0;
    return 1.0 / (2.0 * kPi * max(1e-6, 1.0 - cos_theta_max));
}

// Power heuristic (PBRT uses the squared pdfs).
float power_heuristic(float pdf_a, float pdf_b) {
    const float a2 = pdf_a * pdf_a;
    const float b2 = pdf_b * pdf_b;
    return a2 / max(a2 + b2, 1e-8);
}

// Next-event estimation at a surface with the given BSDF. wo is in the local
// frame and points away from the surface.
float3 sample_direct_light(Bsdf bsdf, float3 position, float3 wo, inout Rng rng) {
    const uint light_count = g_uniforms.counts.y;
    if (light_count == 0)
        return float3(0.0, 0.0, 0.0);

    const uint index = min(uint(rng_float(rng) * float(light_count)), light_count - 1);
    const float selection_pdf = 1.0 / float(light_count);
    const GpuLight light = g_lights[index];
    const float radius = light.params.x;

    const float3 to_light = light.position.xyz - position;
    const float distance = length(to_light);
    if (distance < 1e-4)
        return float3(0.0, 0.0, 0.0);

    if (radius <= 0.0) {
        // Delta point light: light sampling is the only strategy.
        const float3 light_dir = to_light / distance;
        const float3 wi = frame_to_local(bsdf.frame, light_dir);
        if (wi.z <= 0.0)
            return float3(0.0, 0.0, 0.0);
        const float3 f = bsdf_eval(bsdf, wo, wi);
        if (all(f == float3(0.0, 0.0, 0.0)))
            return float3(0.0, 0.0, 0.0);
        if (occluded(position + bsdf.frame.normal * kShadowEpsilon, light_dir, distance - kShadowEpsilon))
            return float3(0.0, 0.0, 0.0);
        return f * light.emission.rgb * wi.z / (distance * distance);
    }

    float sample_distance;
    float cone_pdf;
    const float3 light_dir =
        sample_sphere_light(position, light.position.xyz, radius, rng_float2(rng),
                            sample_distance, cone_pdf);
    const float3 wi = frame_to_local(bsdf.frame, light_dir);
    if (wi.z <= 0.0)
        return float3(0.0, 0.0, 0.0);

    const float3 f = bsdf_eval(bsdf, wo, wi);
    if (all(f == float3(0.0, 0.0, 0.0)))
        return float3(0.0, 0.0, 0.0);
    if (occluded(position + bsdf.frame.normal * kShadowEpsilon, light_dir,
                 sample_distance - kShadowEpsilon))
        return float3(0.0, 0.0, 0.0);

    const float light_pdf = cone_pdf * selection_pdf;
    const float bsdf_pdf = bsdf_pdf(bsdf, wo, wi);
    const float weight = power_heuristic(light_pdf, bsdf_pdf);
    return f * light.emission.rgb * wi.z / light_pdf * weight;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread_id : SV_DispatchThreadID) {
    const uint width = g_uniforms.options.y;
    const uint height = g_uniforms.options.z;
    const uint2 pixel = thread_id.xy;
    if (pixel.x >= width || pixel.y >= height)
        return;

    const uint max_depth = g_uniforms.options.x;
    const uint sample_count = g_uniforms.options.w;
    const CameraData camera = g_uniforms.camera;

    const float2 uv = (float2(pixel) + 0.5) / float2(width, height);
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    Rng rng = rng_create(rng_seed(pixel, g_uniforms.counts.w));

    float3 origin = camera.position.xyz;
    float3 direction = normalize(camera.forward.xyz + camera.right.xyz * ndc.x +
                                 camera.up.xyz * ndc.y);

    float3 radiance = float3(0.0, 0.0, 0.0);
    float3 throughput = float3(1.0, 1.0, 1.0);
    float previous_bsdf_pdf = 1.0;
    bool previous_delta = false;
    bool primary = true;

    for (uint depth = 0; depth < max_depth; ++depth) {
        HitInfo hit;
        if (!trace_scene(origin, direction, 0.001, 1e30, hit)) {
            break;
        }

        if (hit.light_index != kNoLight) {
            if (primary) {
                radiance += throughput * hit.emission;
            } else {
                // MIS weight for the BSDF-sampling strategy that hit this light.
                // A delta bounce cannot be produced by light sampling, so it
                // takes the full contribution.
                const GpuLight light = g_lights[hit.light_index];
                const float light_pdf =
                    sphere_light_pdf(origin, light.position.xyz, light.params.x, direction) /
                    float(g_uniforms.counts.y);
                const float weight =
                    previous_delta ? 1.0 : power_heuristic(previous_bsdf_pdf, light_pdf);
                radiance += throughput * hit.emission * weight;
            }
            break;
        }

        radiance += throughput * hit.emission;

        const Bsdf bsdf = make_bsdf(hit.material, hit.normal);
        const float3 wo = frame_to_local(bsdf.frame, -direction);
        if (wo.z <= 0.0)
            break;

        // Next-event estimation.
        radiance += throughput * sample_direct_light(bsdf, hit.position, wo, rng);

        // BSDF sampling.
        float3 wi;
        float pdf;
        bool sample_delta;
        const float3 f = bsdf_sample(bsdf, wo, rng_float2(rng), wi, pdf, sample_delta);
        if (sample_delta) {
            // Perfect mirror: f already includes the 1/pdf factor (Fresnel).
            throughput *= f;
            previous_bsdf_pdf = 0.0;
            previous_delta = true;
        } else {
            if (pdf <= 0.0 || wi.z <= 0.0)
                break;
            throughput *= f * wi.z / pdf;
            previous_bsdf_pdf = pdf;
            previous_delta = false;
        }

        direction = frame_to_world(bsdf.frame, wi);
        origin = hit.position + hit.normal * kShadowEpsilon;
        primary = false;

        // Russian roulette.
        if (depth >= 3) {
            const float survival = min(0.95, max(throughput.r, max(throughput.g, throughput.b)));
            if (rng_float(rng) > survival)
                break;
            throughput /= survival;
        }
    }

    const float3 previous = g_accumulation[pixel].rgb;
    const float3 accumulated = (previous * float(sample_count) + radiance) / float(sample_count + 1);
    g_accumulation[pixel] = float4(accumulated, 1.0);
}
