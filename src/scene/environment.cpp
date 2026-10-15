#include "scene/environment.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

#include "io/image.h"

namespace vulchovy {

namespace {

float luminance(const vec3& c) {
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

float srgb_to_linear(float c) {
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

} // namespace

Environment load_environment(const std::filesystem::path& path) {
    Environment environment;

    const ImageData image = load_image(path, 3);
    if (!image.valid())
        return environment;

    environment.width = image.width;
    environment.height = image.height;
    environment.pixels.resize(static_cast<size_t>(image.width) * image.height);

    if (image.hdr) {
        for (size_t i = 0; i < environment.pixels.size(); ++i) {
            environment.pixels[i] = vec3{image.floats[i * 3 + 0], image.floats[i * 3 + 1],
                                         image.floats[i * 3 + 2]};
        }
    } else {
        // LDR fallback: expand to float and decode sRGB to linear.
        for (size_t i = 0; i < environment.pixels.size(); ++i) {
            environment.pixels[i] = vec3{srgb_to_linear(image.bytes[i * 3 + 0] / 255.0f),
                                         srgb_to_linear(image.bytes[i * 3 + 1] / 255.0f),
                                         srgb_to_linear(image.bytes[i * 3 + 2] / 255.0f)};
        }
    }

    return environment;
}

EnvironmentSampling build_environment_sampling(const Environment& environment) {
    EnvironmentSampling sampling;
    if (!environment.valid())
        return sampling;

    const uint32_t width = environment.width;
    const uint32_t height = environment.height;
    const float pi = glm::pi<float>();

    std::vector<float> weights(static_cast<size_t>(width) * height);
    std::vector<double> row_sums(height, 0.0);
    double total = 0.0;
    for (uint32_t y = 0; y < height; ++y) {
        const float sin_theta = std::sin((static_cast<float>(y) + 0.5f) /
                                         static_cast<float>(height) * pi);
        for (uint32_t x = 0; x < width; ++x) {
            const size_t index = static_cast<size_t>(y) * width + x;
            const float weight =
                std::max(0.0f, luminance(environment.pixels[index])) * sin_theta;
            weights[index] = weight;
            row_sums[y] += weight;
            total += weight;
        }
    }

    sampling.marginal.assign(height + 1, 0.0f);
    sampling.conditional.assign(static_cast<size_t>(height) * (width + 1), 0.0f);

    // Conditional CDF within each row.
    for (uint32_t y = 0; y < height; ++y) {
        const float inv_row = row_sums[y] > 0.0 ? static_cast<float>(1.0 / row_sums[y]) : 0.0f;
        float accum = 0.0f;
        const size_t base = static_cast<size_t>(y) * (width + 1);
        sampling.conditional[base] = 0.0f;
        for (uint32_t x = 0; x < width; ++x) {
            accum += weights[static_cast<size_t>(y) * width + x] * inv_row;
            sampling.conditional[base + x + 1] = accum;
        }
        // Guard against a fully black row (avoid a flat, non-monotonic CDF).
        if (row_sums[y] <= 0.0) {
            for (uint32_t x = 0; x <= width; ++x)
                sampling.conditional[base + x] = static_cast<float>(x) / static_cast<float>(width);
        }
    }

    // Marginal CDF over rows.
    if (total > 0.0) {
        double accum = 0.0;
        for (uint32_t y = 0; y < height; ++y) {
            accum += row_sums[y];
            sampling.marginal[y + 1] = static_cast<float>(accum / total);
        }
        sampling.total_weight = static_cast<float>(total);
        const double denom = 2.0 * static_cast<double>(pi) * static_cast<double>(pi) * total;
        sampling.pdf_scale = static_cast<float>(
            static_cast<double>(width) * static_cast<double>(height) / denom);
    } else {
        for (uint32_t y = 0; y <= height; ++y)
            sampling.marginal[y] = static_cast<float>(y) / static_cast<float>(height);
    }

    return sampling;
}

Environment make_black_environment() {
    Environment environment;
    environment.width = 1;
    environment.height = 1;
    environment.pixels.assign(1, vec3{0.0f});
    return environment;
}

} // namespace vulchovy
