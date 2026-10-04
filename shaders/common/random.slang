#pragma once

// Small PCG-based random number generator for the compute kernels.

struct Rng {
    uint state;
};

Rng rng_create(uint seed) {
    Rng rng;
    rng.state = seed;
    return rng;
}

uint rng_uint(inout Rng rng) {
    rng.state = rng.state * 747796405u + 2891336453u;
    uint word = ((rng.state >> ((rng.state >> 28u) + 4u)) ^ rng.state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rng_float(inout Rng rng) {
    return float(rng_uint(rng)) * (1.0 / 4294967296.0);
}

float2 rng_float2(inout Rng rng) {
    return float2(rng_float(rng), rng_float(rng));
}

/// @brief Seed derived from a pixel and the frame index.
uint rng_seed(uint2 pixel, uint frame) {
    uint seed = pixel.x * 1973u + pixel.y * 9277u + frame * 26699u;
    return seed | 1u;
}
