#include "scene/environment.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/constants.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace vulchovy {

namespace {

float luminance(const vec3& c) {
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

} // namespace

Environment load_environment(const std::filesystem::path& path) {
    Environment environment;

    int width = 0;
    int height = 0;
    int channels = 0;
    float* data = stbi_loadf(path.string().c_str(), &width, &height, &channels, STBI_rgb);
    if (data == nullptr || width <= 0 || height <= 0) {
        std::fprintf(stderr, "vulchovy: could not load environment '%s': %s\n",
                     path.string().c_str(), stbi_failure_reason());
        if (data != nullptr)
            stbi_image_free(data);
        return environment;
    }

    environment.width = static_cast<uint32_t>(width);
    environment.height = static_cast<uint32_t>(height);
    environment.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
    for (size_t i = 0; i < environment.pixels.size(); ++i) {
        environment.pixels[i] = vec3{data[i * 3 + 0], data[i * 3 + 1], data[i * 3 + 2]};
    }

    stbi_image_free(data);
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

Environment make_procedural_sky() {
    Environment environment;
    const uint32_t width = 512;
    const uint32_t height = 256;
    environment.width = width;
    environment.height = height;
    environment.pixels.resize(static_cast<size_t>(width) * height);

    const float pi = glm::pi<float>();
    for (uint32_t y = 0; y < height; ++y) {
        const float theta = (static_cast<float>(y) + 0.5f) / static_cast<float>(height) * pi;
        const float up = std::cos(theta); // +1 at the top, -1 at the bottom
        for (uint32_t x = 0; x < width; ++x) {
            const float phi = (static_cast<float>(x) + 0.5f) / static_cast<float>(width) *
                              2.0f * pi;
            const vec3 direction{std::sin(theta) * std::cos(phi), up,
                                 std::sin(theta) * std::sin(phi)};
            const float horizon = std::max(0.0f, up);
            vec3 sky = glm::mix(vec3{0.55f, 0.62f, 0.75f}, vec3{0.16f, 0.32f, 0.72f}, horizon);
            const float sun = std::pow(std::max(0.0f, glm::dot(direction, normalize(vec3{0.4f, 0.6f, 0.5f}))), 256.0f);
            sky += vec3{6.0f, 5.4f, 4.6f} * sun;
            environment.pixels[static_cast<size_t>(y) * width + x] = sky;
        }
    }
    return environment;
}

} // namespace vulchovy
