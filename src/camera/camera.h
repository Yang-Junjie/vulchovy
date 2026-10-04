#pragma once

#include "core/math.h"

namespace vulchovy {

/// @brief A ray in world space.
struct Ray {
    vec3 origin{0.0f};
    vec3 direction{0.0f, 0.0f, -1.0f};
};

/// @brief GPU representation of a camera's ray basis.
///
/// Mirrors the `CameraData` struct in the shading kernels. Kept as plain vec4s
/// so the std140 layout matches without explicit padding.
struct CameraData {
    vec4 position; // xyz = eye
    vec4 forward;  // xyz = unit forward
    vec4 right;    // xyz = right * tan(fov/2) * aspect
    vec4 up;       // xyz = up * tan(fov/2)
};

/// @brief Abstract camera.
///
/// The GPU path consumes gpu_data(); the CPU path (picking, tests, future CPU
/// integrators) consumes ray(). Concrete models decide how a ray is formed.
class Camera {
public:
    virtual ~Camera() = default;

    virtual void set_aspect(float aspect) = 0;
    virtual CameraData gpu_data() const = 0;

    /// @brief Ray through normalized device coordinates in [-1, 1], y up.
    virtual Ray ray(vec2 ndc) const = 0;
};

} // namespace vulchovy
