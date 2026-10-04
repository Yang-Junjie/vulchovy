#pragma once

// Physically based BSDF, structured after PBRT: a shading frame, a GGX
// microfacet distribution sampled through the visible-normal distribution
// (Heitz 2018), Schlick Fresnel and Smith masking-shadowing, combined with a
// Lambertian diffuse lobe for the metallic-roughness material model.

#include "common/scene.slang"

static const float kPi = 3.14159265358979;

float luminance(float3 c) {
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// --- shading frame ----------------------------------------------------------

struct Frame {
    float3 tangent;
    float3 bitangent;
    float3 normal;
};

Frame frame_from_normal(float3 n) {
    Frame frame;
    frame.normal = n;
    if (abs(n.z) < 0.999)
        frame.tangent = normalize(cross(float3(0.0, 0.0, 1.0), n));
    else
        frame.tangent = normalize(cross(float3(1.0, 0.0, 0.0), n));
    frame.bitangent = cross(n, frame.tangent);
    return frame;
}

float3 frame_to_local(Frame frame, float3 v) {
    return float3(dot(v, frame.tangent), dot(v, frame.bitangent), dot(v, frame.normal));
}

float3 frame_to_world(Frame frame, float3 v) {
    return v.x * frame.tangent + v.y * frame.bitangent + v.z * frame.normal;
}

// Builds an orthonormal basis around a unit vector (used for cone sampling).
void make_basis(float3 n, out float3 tangent, out float3 bitangent) {
    if (abs(n.z) < 0.999)
        tangent = normalize(cross(float3(0.0, 0.0, 1.0), n));
    else
        tangent = normalize(cross(float3(1.0, 0.0, 0.0), n));
    bitangent = cross(n, tangent);
}

// --- material to BSDF -------------------------------------------------------

struct Bsdf {
    Frame frame;
    float3 diffuse_albedo;
    float3 f0;
    float alpha;            // roughness squared
    float p_spec;           // probability of choosing the specular lobe when sampling
    bool specular_is_delta; // roughness below the threshold: perfect mirror
};

Bsdf make_bsdf(GpuMaterial material, float3 normal) {
    Bsdf bsdf;
    bsdf.frame = frame_from_normal(normal);

    const float metallic = clamp(material.params.x, 0.0, 1.0);
    const float roughness = clamp(material.params.y, 0.0, 1.0);
    bsdf.specular_is_delta = roughness < 1e-3;
    bsdf.alpha = max(roughness * roughness, 1e-6);
    bsdf.diffuse_albedo = material.base_color.rgb * (1.0 - metallic);
    bsdf.f0 = lerp(float3(0.04, 0.04, 0.04), material.base_color.rgb, metallic);

    const float specular_weight = luminance(bsdf.f0);
    const float diffuse_weight = luminance(bsdf.diffuse_albedo);
    bsdf.p_spec = clamp(specular_weight / max(specular_weight + diffuse_weight, 1e-3), 0.1, 0.9);
    return bsdf;
}

// --- microfacet terms (local frame, z = macro normal) -----------------------

float3 fresnel_schlick(float cos_theta, float3 f0) {
    const float m = pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
    return f0 + (1.0 - f0) * m;
}

float ggx_d(float cos_theta_m, float alpha) {
    const float a2 = alpha * alpha;
    const float d = cos_theta_m * cos_theta_m * (a2 - 1.0) + 1.0;
    return a2 / (kPi * d * d);
}

float ggx_lambda(float cos_theta, float alpha) {
    const float c2 = clamp(cos_theta * cos_theta, 1e-8, 1.0);
    const float tan2 = (1.0 - c2) / c2;
    return 0.5 * (sqrt(1.0 + alpha * alpha * tan2) - 1.0);
}

float ggx_g1(float cos_theta, float alpha) {
    return 1.0 / (1.0 + ggx_lambda(cos_theta, alpha));
}

// Visible-normal-distribution sampling (Heitz 2018), wo in the local frame.
float3 ggx_sample_half(float3 wo, float alpha, float2 u) {
    const float3 vh = normalize(float3(alpha * wo.x, alpha * wo.y, wo.z));
    const float lensq = vh.x * vh.x + vh.y * vh.y;
    const float3 t1 = lensq > 0.0 ? float3(-vh.y, vh.x, 0.0) * rsqrt(lensq) : float3(1.0, 0.0, 0.0);
    const float3 t2 = cross(vh, t1);

    const float r = sqrt(u.x);
    const float phi = 2.0 * kPi * u.y;
    float p1 = r * cos(phi);
    float p2 = r * sin(phi);
    const float s = 0.5 * (1.0 + vh.z);
    p2 = (1.0 - s) * sqrt(max(0.0, 1.0 - p1 * p1)) + s * p2;

    const float3 nh = p1 * t1 + p2 * t2 + sqrt(max(0.0, 1.0 - p1 * p1 - p2 * p2)) * vh;
    return normalize(float3(alpha * nh.x, alpha * nh.y, max(0.0, nh.z)));
}

// --- combined BSDF ----------------------------------------------------------

float3 bsdf_eval(Bsdf bsdf, float3 wo, float3 wi) {
    if (wo.z <= 0.0 || wi.z <= 0.0)
        return float3(0.0, 0.0, 0.0);

    float3 f = bsdf.diffuse_albedo / kPi;

    // A delta specular lobe has measure zero away from the mirror direction, so
    // it contributes nothing to an evaluated direction (only to sampling).
    if (!bsdf.specular_is_delta) {
        const float3 h = normalize(wo + wi);
        if (h.z > 0.0) {
            const float3 fr = fresnel_schlick(dot(wo, h), bsdf.f0);
            const float d = ggx_d(h.z, bsdf.alpha);
            const float g = ggx_g1(wo.z, bsdf.alpha) * ggx_g1(wi.z, bsdf.alpha);
            f += fr * d * g / (4.0 * wo.z * wi.z);
        }
    }
    return f;
}

float bsdf_pdf(Bsdf bsdf, float3 wo, float3 wi) {
    if (wo.z <= 0.0 || wi.z <= 0.0)
        return 0.0;

    const float diffuse_pdf = wi.z / kPi;
    float specular_pdf = 0.0;
    if (!bsdf.specular_is_delta) {
        const float3 h = normalize(wo + wi);
        if (h.z > 0.0)
            specular_pdf = ggx_d(h.z, bsdf.alpha) * ggx_g1(wo.z, bsdf.alpha) / (4.0 * wo.z);
    }

    return (1.0 - bsdf.p_spec) * diffuse_pdf + bsdf.p_spec * specular_pdf;
}

float3 cosine_sample_hemisphere_local(float2 u) {
    const float r = sqrt(u.x);
    const float phi = 2.0 * kPi * u.y;
    return float3(r * cos(phi), r * sin(phi), sqrt(max(0.0, 1.0 - u.x)));
}

// Samples the BSDF, returning the value and the (combined) pdf. The direction
// is in the local frame. For a delta (perfect mirror) sample, delta is true,
// pdf is 0 and the returned value is already the throughput multiplier
// (the Fresnel reflectance), so the caller does not divide by a pdf.
float3 bsdf_sample(Bsdf bsdf, float3 wo, float2 u, out float3 wi, out float pdf, out bool delta) {
    delta = false;

    if (u.x < bsdf.p_spec) {
        if (bsdf.specular_is_delta) {
            wi = float3(-wo.x, -wo.y, wo.z);
            if (wi.z <= 0.0) {
                pdf = 0.0;
                return float3(0.0, 0.0, 0.0);
            }
            delta = true;
            pdf = 0.0;
            return fresnel_schlick(wo.z, bsdf.f0);
        }

        const float2 us = float2(min(u.x / max(bsdf.p_spec, 1e-6), 1.0 - 1e-6), u.y);
        const float3 h = ggx_sample_half(wo, bsdf.alpha, us);
        wi = -wo + 2.0 * dot(wo, h) * h;
    } else {
        const float2 ud = float2((u.x - bsdf.p_spec) / max(1.0 - bsdf.p_spec, 1e-6), u.y);
        wi = cosine_sample_hemisphere_local(ud);
    }

    if (wi.z <= 0.0) {
        pdf = 0.0;
        return float3(0.0, 0.0, 0.0);
    }

    pdf = bsdf_pdf(bsdf, wo, wi);
    return bsdf_eval(bsdf, wo, wi);
}
