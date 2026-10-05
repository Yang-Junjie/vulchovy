#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "core/math.h"

namespace vulchovy {

/// @brief CPU-side HDR environment map in equirectangular parametrisation.
///
/// Pixels are linear RGB, row-major, @c width * @c height entries; row 0 is the
/// top of the map (the +Y pole). @c intensity and @c rotation are applied on the
/// GPU and do not bake into the pixels.
struct Environment {
    std::vector<vec3> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    float intensity = 1.0f;
    float rotation = 0.0f; // yaw around +Y, radians

    bool valid() const { return width > 0 && height > 0 && !pixels.empty(); }
};

/// @brief Importance-sampling data derived from an environment map.
///
/// The conditional CDF is stored row-major with @c width + 1 entries per row
/// (row r starts at index @c r * (width + 1)); the marginal CDF has one entry
/// per row plus a final 1. Weights are luminance * sin(theta) so that sampling
/// is proportional to the solid-angle radiance.
struct EnvironmentSampling {
    std::vector<float> marginal;    // height + 1 entries
    std::vector<float> conditional; // height * (width + 1) entries
    float total_weight = 0.0f;
    float pdf_scale = 0.0f; // width * height / (2 * pi^2 * total_weight)
};

/// @brief Loads an equirectangular HDR (Radiance .hdr) through stb_image.
///
/// Returns an invalid environment (and logs to stderr) when the file is missing
/// or malformed, so the caller can fall back to a procedural sky.
Environment load_environment(const std::filesystem::path& path);

/// @brief Builds the CDF used to importance-sample the map.
EnvironmentSampling build_environment_sampling(const Environment& environment);

/// @brief A small procedural sky used when no HDR file is available.
Environment make_procedural_sky();

} // namespace vulchovy
