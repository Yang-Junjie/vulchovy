#pragma once

// Equirectangular HDR environment lighting with importance sampling.
//
// The image and its marginal/conditional CDFs live in the shared scene set
// (set 0, bindings 5-7). Directions are mapped to (u, v) with u around +Y and
// v from the +Y pole to the -Y pole; a yaw rotation lets the artist turn the
// environment.

#include "common/scene.slang"

[[vk::binding(5, 0)]] Sampler2D g_environment;
[[vk::binding(6, 0)]] StructuredBuffer<float> g_env_marginal;    // height + 1
[[vk::binding(7, 0)]] StructuredBuffer<float> g_env_conditional; // height * (width + 1)

float env_luminance(float3 c) {
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float3 rotate_y(float3 d, float angle) {
    const float c = cos(angle);
    const float s = sin(angle);
    return float3(c * d.x + s * d.z, d.y, -s * d.x + c * d.z);
}

// World direction -> equirectangular uv (applies the inverse rotation).
float2 env_direction_to_uv(float3 direction, float rotation) {
    const float3 d = rotate_y(direction, -rotation);
    const float u = atan2(d.z, d.x) / (2.0 * kPi) + 0.5;
    const float v = acos(clamp(d.y, -1.0, 1.0)) / kPi;
    return float2(u, v);
}

// Equirectangular uv -> world direction (applies the rotation).
float3 env_uv_to_direction(float2 uv, float rotation) {
    const float phi = (uv.x - 0.5) * 2.0 * kPi;
    const float theta = uv.y * kPi;
    const float sin_theta = sin(theta);
    const float3 d = float3(sin_theta * cos(phi), cos(theta), sin_theta * sin(phi));
    return normalize(rotate_y(d, rotation));
}

float3 env_radiance(float3 direction, float intensity, float rotation) {
    const float2 uv = env_direction_to_uv(direction, rotation);
    return g_environment.SampleLevel(uv, 0.0).rgb * intensity;
}

// Solid-angle pdf of the importance-sampling distribution, not including any
// strategy selection probability.
float env_pdf(float3 direction, float rotation, float pdf_scale) {
    const float3 raw =
        g_environment.SampleLevel(env_direction_to_uv(direction, rotation), 0.0).rgb;
    return env_luminance(raw) * pdf_scale;
}

// Samples a direction with density proportional to luminance * sin(theta).
// Returns the world-space direction; pdf is the solid-angle pdf.
float3 env_sample(float2 u, float rotation, float pdf_scale, out float pdf) {
    uint width;
    uint height;
    g_environment.GetDimensions(width, height);

    // Marginal CDF -> row.
    uint lo = 0;
    uint hi = height;
    while (lo + 1 < hi) {
        const uint mid = (lo + hi) / 2;
        if (g_env_marginal[mid] <= u.x)
            lo = mid;
        else
            hi = mid;
    }
    const uint row = min(lo, height - 1);
    const float m0 = g_env_marginal[row];
    const float m1 = g_env_marginal[row + 1];
    const float fy = m1 > m0 ? (u.x - m0) / (m1 - m0) : 0.0;

    // Conditional CDF -> column within the row.
    const uint base = row * (width + 1);
    uint clo = 0;
    uint chi = width;
    while (clo + 1 < chi) {
        const uint mid = (clo + chi) / 2;
        if (g_env_conditional[base + mid] <= u.y)
            clo = mid;
        else
            chi = mid;
    }
    const uint col = min(clo, width - 1);
    const float c0 = g_env_conditional[base + col];
    const float c1 = g_env_conditional[base + col + 1];
    const float fx = c1 > c0 ? (u.y - c0) / (c1 - c0) : 0.0;

    const float2 uv =
        float2((float(col) + fx) / float(width), (float(row) + fy) / float(height));
    const float3 raw = g_environment.SampleLevel(uv, 0.0).rgb;
    pdf = env_luminance(raw) * pdf_scale;
    return env_uv_to_direction(uv, rotation);
}
