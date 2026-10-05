#pragma once

// Physically based BSDF, structured after PBRT: a shading frame, a GGX
// microfacet distribution sampled through the visible-normal distribution
// (Heitz 2018), Schlick Fresnel for conductors, and height-correlated Smith
// masking-shadowing. Opaque surfaces combine a Lambertian diffuse lobe with a
// GGX conductor lobe (metallic-roughness); transmissive surfaces use a
// microfacet dielectric (Walter et al. 2007) with an optional smooth/delta
// special case.

#include "common/scene.slang"
#include "common/random.slang"

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

// Builds a frame whose tangent is the anisotropic tangent direction, projected
// onto the plane perpendicular to n.
Frame frame_from_normal_tangent(float3 n, float3 t) {
    Frame frame;
    frame.normal = n;
    frame.tangent = normalize(t - n * dot(n, t));
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
    float2 alpha;           // per-axis roughness squared (x = tangent, y = bitangent)
    float p_spec;           // probability of choosing the specular lobe when sampling
    bool specular_is_delta; // roughness below the threshold: perfect mirror
    float3 emission;        // textured emissive radiance
    // Dielectric transmission (glass).
    bool transmissive;
    float eta;              // eta_i / eta_t for the current side
    float3 transmission_color;
};

// World-space tangent used to orient anisotropic highlights. For the spheres in
// the default scene this is the azimuthal direction, so the highlight stretches
// around the latitude lines. It is degenerate only exactly at the poles, where
// we fall back to a different reference axis; the fallback uses a very small
// threshold so it does not introduce a visible discontinuity ring near the pole.
float3 anisotropic_tangent(float3 n) {
    float3 t = cross(float3(0.0, 1.0, 0.0), n);
    if (dot(t, t) < 1e-6)
        t = cross(float3(1.0, 0.0, 0.0), n);
    return normalize(t);
}

Bsdf make_bsdf(GpuMaterial material, float3 normal, float3 tangent, bool front_face, float2 uv) {
    Bsdf bsdf;

    const float transmission = clamp(material.params.z, 0.0, 1.0);
    const float ior = max(material.params.w, 1.0);
    const float anisotropy = clamp(material.params2.x, 0.0, 0.95);

    // Metallic-roughness map (G = roughness, B = metallic), multiplied by the
    // scalar factors.
    const float3 mr = sample_texture_or_white(material.textures.y, uv);
    const float metallic = clamp(material.params.x * mr.b, 0.0, 1.0);
    const float roughness = clamp(material.params.y * mr.g, 0.0, 1.0);

    // Base colour and emission are factor * texture (the texture slot returns
    // white when unused, so the factor passes through).
    const float3 base_color = material.base_color.rgb * sample_texture_or_white(material.textures.x, uv);
    bsdf.emission = material.emission.rgb * sample_texture_or_white(material.textures.w, uv);

    // Normal mapping perturbs the shading normal through the tangent frame.
    float3 shading_normal = normal;
    if (material.textures.z >= 0) {
        const float3 n_ts = g_textures[material.textures.z].SampleLevel(uv, 0.0).xyz * 2.0 - 1.0;
        float3 t = tangent - normal * dot(normal, tangent);
        if (dot(t, t) < 1e-8)
            t = anisotropic_tangent(normal);
        else
            t = normalize(t);
        const float3 b = cross(normal, t);
        shading_normal = normalize(t * n_ts.x + b * n_ts.y + normal * n_ts.z);
    }

    bsdf.frame = frame_from_normal_tangent(shading_normal, anisotropic_tangent(shading_normal));

    const float alpha_iso = max(roughness * roughness, 1e-6);
    const float aspect = sqrt(1.0 - anisotropy * 0.9);
    bsdf.alpha = float2(max(alpha_iso / aspect, 1e-4), max(alpha_iso * aspect, 1e-4));
    bsdf.specular_is_delta = roughness < 1e-3 && anisotropy < 1e-3;

    bsdf.transmissive = transmission > 0.0 && metallic <= 0.0;
    if (bsdf.transmissive) {
        // eta_i / eta_t: entering (front face) from air, or exiting from the glass.
        bsdf.eta = front_face ? (1.0 / ior) : ior;
        bsdf.transmission_color = base_color;
        bsdf.diffuse_albedo = float3(0.0, 0.0, 0.0);
        bsdf.f0 = float3(0.0, 0.0, 0.0);
        bsdf.p_spec = 1.0;
        return bsdf;
    }

    bsdf.transmissive = false;
    bsdf.eta = 1.0;
    bsdf.transmission_color = float3(0.0, 0.0, 0.0);
    bsdf.diffuse_albedo = base_color * (1.0 - metallic);
    bsdf.f0 = lerp(float3(0.04, 0.04, 0.04), base_color, metallic);

    const float specular_weight = luminance(bsdf.f0);
    const float diffuse_weight = luminance(bsdf.diffuse_albedo);
    bsdf.p_spec = clamp(specular_weight / max(specular_weight + diffuse_weight, 1e-3), 0.1, 0.9);
    return bsdf;
}

// Unpolarized Fresnel reflectance of a dielectric. eta = eta_i / eta_t.
float fresnel_dielectric(float cos_i, float eta) {
    cos_i = clamp(cos_i, 0.0, 1.0);
    const float sin2_t = eta * eta * (1.0 - cos_i * cos_i);
    if (sin2_t >= 1.0)
        return 1.0; // total internal reflection
    const float cos_t = sqrt(1.0 - sin2_t);
    const float rs = (eta * cos_i - cos_t) / (eta * cos_i + cos_t);
    const float rp = (cos_i - eta * cos_t) / (cos_i + eta * cos_t);
    return 0.5 * (rs * rs + rp * rp);
}

// --- microfacet terms (local frame, z = macro normal) -----------------------

float3 fresnel_schlick(float cos_theta, float3 f0) {
    const float m = pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
    return f0 + (1.0 - f0) * m;
}

float ggx_d(float3 h, float2 alpha) {
    const float ax2 = alpha.x * alpha.x;
    const float ay2 = alpha.y * alpha.y;
    const float d = (h.x * h.x) / ax2 + (h.y * h.y) / ay2 + h.z * h.z;
    return 1.0 / (kPi * alpha.x * alpha.y * d * d);
}

float ggx_lambda(float3 w, float2 alpha) {
    const float sin2 = max(1.0 - w.z * w.z, 0.0);
    const float cos2 = max(w.z * w.z, 1e-8);
    const float alpha_o2 = (w.x * w.x * alpha.x * alpha.x + w.y * w.y * alpha.y * alpha.y) /
                           max(sin2, 1e-10);
    const float tan2 = sin2 / cos2;
    return 0.5 * (sqrt(1.0 + alpha_o2 * tan2) - 1.0);
}

float ggx_g1(float3 w, float2 alpha) {
    return 1.0 / (1.0 + ggx_lambda(w, alpha));
}

// Height-correlated Smith masking-shadowing (PBRT).
float ggx_g2(float3 wo, float3 wi, float2 alpha) {
    return 1.0 / (1.0 + ggx_lambda(wo, alpha) + ggx_lambda(wi, alpha));
}

// Visible-normal-distribution pdf of the half vector (PBRT TrowbridgeReitz::PDF).
float ggx_pdf_h(float3 wo, float3 h, float2 alpha) {
    return ggx_d(h, alpha) * ggx_g1(wo, alpha) * abs(dot(wo, h)) / max(wo.z, 1e-6);
}

// Visible-normal-distribution sampling (Heitz 2018), wo in the local frame.
float3 ggx_sample_half(float3 wo, float2 alpha, float2 u) {
    const float3 vh = normalize(float3(alpha.x * wo.x, alpha.y * wo.y, wo.z));
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
    return normalize(float3(alpha.x * nh.x, alpha.y * nh.y, max(0.0, nh.z)));
}

// --- microfacet dielectric (Walter et al. 2007, as in PBRT) -----------------
//
// Local frame; wo.z > 0. pbrt_eta = eta_t / eta_i, the reciprocal of Bsdf::eta.

// PBRT's Refract; returns false on total internal reflection.
bool mf_refract(float3 wo, float3 wm, float pbrt_eta, out float etap, out float3 wi) {
    float eta = pbrt_eta;
    float3 n = wm;
    float cos_i = dot(n, wo);
    if (cos_i < 0.0) {
        eta = 1.0 / eta;
        cos_i = -cos_i;
        n = -n;
    }
    const float sin2_i = max(0.0, 1.0 - cos_i * cos_i);
    const float sin2_t = sin2_i / (eta * eta);
    if (sin2_t >= 1.0) {
        etap = eta;
        wi = float3(0.0, 0.0, 1.0);
        return false;
    }
    const float cos_t = sqrt(1.0 - sin2_t);
    wi = -wo / eta + (cos_i / eta - cos_t) * n;
    etap = eta;
    return true;
}

float3 dielectric_eval(Bsdf bsdf, float3 wo, float3 wi) {
    const float pbrt_eta = 1.0 / bsdf.eta;
    const float cos_o = wo.z;
    const float cos_i = wi.z;
    const bool reflect = cos_i * cos_o > 0.0;
    const float etap = reflect ? 1.0 : pbrt_eta;

    float3 wm = wi * etap + wo;
    if (dot(wm, wm) == 0.0)
        return float3(0.0, 0.0, 0.0);
    wm = normalize(wm);
    if (wm.z < 0.0)
        wm = -wm;
    if (dot(wm, wi) * cos_i < 0.0 || dot(wm, wo) * cos_o < 0.0)
        return float3(0.0, 0.0, 0.0);

    const float F = fresnel_dielectric(abs(dot(wo, wm)), bsdf.eta);
    const float d = ggx_d(wm, bsdf.alpha);
    const float g = ggx_g2(wo, wi, bsdf.alpha);
    if (reflect)
        return d * g * F / abs(4.0 * cos_i * cos_o);

    const float denom = pow(dot(wi, wm) + dot(wo, wm) / etap, 2.0) * cos_i * cos_o;
    float3 ft = d * (1.0 - F) * g * abs(dot(wi, wm) * dot(wo, wm) / denom);
    return bsdf.transmission_color * ft / (etap * etap);
}

float dielectric_pdf(Bsdf bsdf, float3 wo, float3 wi) {
    const float pbrt_eta = 1.0 / bsdf.eta;
    const float cos_o = wo.z;
    const float cos_i = wi.z;
    const bool reflect = cos_i * cos_o > 0.0;
    const float etap = reflect ? 1.0 : pbrt_eta;

    float3 wm = wi * etap + wo;
    if (dot(wm, wm) == 0.0)
        return 0.0;
    wm = normalize(wm);
    if (wm.z < 0.0)
        wm = -wm;
    if (dot(wm, wi) * cos_i < 0.0 || dot(wm, wo) * cos_o < 0.0)
        return 0.0;

    const float F = fresnel_dielectric(abs(dot(wo, wm)), bsdf.eta);
    if (reflect)
        return ggx_pdf_h(wo, wm, bsdf.alpha) / (4.0 * abs(dot(wo, wm))) * F;

    const float denom = pow(dot(wi, wm) + dot(wo, wm) / etap, 2.0);
    const float dwm_dwi = abs(dot(wi, wm)) / denom;
    return ggx_pdf_h(wo, wm, bsdf.alpha) * dwm_dwi * (1.0 - F);
}

// --- combined BSDF ----------------------------------------------------------

float3 bsdf_eval(Bsdf bsdf, float3 wo, float3 wi) {
    if (bsdf.transmissive) {
        if (bsdf.specular_is_delta)
            return float3(0.0, 0.0, 0.0);
        return dielectric_eval(bsdf, wo, wi);
    }

    if (wo.z <= 0.0 || wi.z <= 0.0)
        return float3(0.0, 0.0, 0.0);

    float3 f = bsdf.diffuse_albedo / kPi;

    // A delta specular lobe has measure zero away from the mirror direction, so
    // it contributes nothing to an evaluated direction (only to sampling).
    if (!bsdf.specular_is_delta) {
        const float3 h = normalize(wo + wi);
        if (h.z > 0.0) {
            const float3 fr = fresnel_schlick(dot(wo, h), bsdf.f0);
            const float d = ggx_d(h, bsdf.alpha);
            const float g = ggx_g2(wo, wi, bsdf.alpha);
            f += fr * d * g / (4.0 * wo.z * wi.z);
        }
    }
    return f;
}

float bsdf_pdf(Bsdf bsdf, float3 wo, float3 wi) {
    if (bsdf.transmissive) {
        if (bsdf.specular_is_delta)
            return 0.0;
        return dielectric_pdf(bsdf, wo, wi);
    }

    if (wo.z <= 0.0 || wi.z <= 0.0)
        return 0.0;

    const float diffuse_pdf = wi.z / kPi;
    float specular_pdf = 0.0;
    if (!bsdf.specular_is_delta) {
        const float3 h = normalize(wo + wi);
        if (h.z > 0.0)
            specular_pdf = ggx_d(h, bsdf.alpha) * ggx_g1(wo, bsdf.alpha) / (4.0 * wo.z);
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
//
// GGX reflection can produce a direction below the surface; those samples are
// rejected and redrawn (PBRT does the same), otherwise the path would be
// terminated and leave black speckles at grazing angles.
float3 bsdf_sample(Bsdf bsdf, float3 wo, inout Rng rng, out float3 wi, out float pdf, out bool delta) {
    delta = false;

    if (bsdf.transmissive) {
        if (bsdf.specular_is_delta) {
            const float eta = bsdf.eta;
            const float reflectance = fresnel_dielectric(wo.z, eta);
            if (rng_float(rng) < reflectance) {
                wi = float3(-wo.x, -wo.y, wo.z);
                delta = true;
                pdf = 0.0;
                return float3(1.0, 1.0, 1.0);
            }

            const float k = 1.0 - eta * eta * (1.0 - wo.z * wo.z);
            if (k < 0.0) {
                // Total internal reflection.
                wi = float3(-wo.x, -wo.y, wo.z);
                delta = true;
                pdf = 0.0;
                return float3(1.0, 1.0, 1.0);
            }

            const float cos_t = sqrt(k);
            wi = float3(-eta * wo.x, -eta * wo.y, -cos_t);
            delta = true;
            pdf = 0.0;
            return bsdf.transmission_color * (1.0 / (eta * eta));
        }

        // Rough dielectric: sample the visible normal distribution, then reflect
        // or refract about the microfacet normal (Walter et al. 2007).
        const float3 wm = ggx_sample_half(wo, bsdf.alpha, rng_float2(rng));
        const float F = fresnel_dielectric(abs(dot(wo, wm)), bsdf.eta);
        if (rng_float(rng) < F) {
            wi = -wo + 2.0 * dot(wo, wm) * wm;
            if (wi.z <= 0.0) {
                wi = float3(0.0, 0.0, 1.0);
                pdf = 0.0;
                return float3(0.0, 0.0, 0.0);
            }
            pdf = ggx_pdf_h(wo, wm, bsdf.alpha) / (4.0 * abs(dot(wo, wm))) * F;
            return dielectric_eval(bsdf, wo, wi);
        }

        float etap;
        float3 refracted;
        if (!mf_refract(wo, wm, 1.0 / bsdf.eta, etap, refracted) || refracted.z >= 0.0) {
            wi = float3(0.0, 0.0, 1.0);
            pdf = 0.0;
            return float3(0.0, 0.0, 0.0);
        }
        wi = refracted;
        const float denom = pow(dot(wi, wm) + dot(wo, wm) / etap, 2.0);
        pdf = ggx_pdf_h(wo, wm, bsdf.alpha) * abs(dot(wi, wm)) / denom * (1.0 - F);
        return dielectric_eval(bsdf, wo, wi);
    }

    for (int attempt = 0; attempt < 8; ++attempt) {
        const float2 u = rng_float2(rng);

        if (u.x < bsdf.p_spec) {
            if (bsdf.specular_is_delta) {
                wi = float3(-wo.x, -wo.y, wo.z);
                if (wi.z <= 0.0)
                    break;
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

        if (wi.z > 0.0) {
            pdf = bsdf_pdf(bsdf, wo, wi);
            return bsdf_eval(bsdf, wo, wi);
        }
    }

    wi = float3(0.0, 0.0, 1.0);
    pdf = 0.0;
    return float3(0.0, 0.0, 0.0);
}
