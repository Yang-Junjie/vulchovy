#pragma once

#include "camera/camera.h"
#include "core/math.h"

namespace vulchovy {

// Mirrored by the `Uniforms` struct in the whitted kernels (set 1, binding 0).
struct Uniforms {
    CameraData camera;
    glm::uvec4 counts;  // x = material count, y = light count, z = instance count, w = frame
    glm::uvec4 options; // x = max depth, y = width, z = height
};

static_assert(sizeof(Uniforms) == 96, "Uniforms must match the std140 shader block");

} // namespace vulchovy
