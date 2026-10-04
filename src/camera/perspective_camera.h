#pragma once

#include "camera/camera.h"

namespace vulchovy {

/// @brief Pinhole (perspective) camera.
///
/// Poses are described by a look-at triple; the ray basis is derived on demand.
class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera() = default;

    void set_pose(const vec3& position, const vec3& target, const vec3& up = vec3{0.0f, 1.0f, 0.0f});
    void set_vertical_fov(float degrees);
    void set_aspect(float aspect) override;

    const vec3& position() const { return position_; }
    const vec3& target() const { return target_; }
    const vec3& up() const { return up_; }
    float vertical_fov_degrees() const { return vertical_fov_degrees_; }
    float aspect() const { return aspect_; }

    vec3 forward() const;
    vec3 right() const;
    vec3 true_up() const;
    float tan_half_fov() const;

    CameraData gpu_data() const override;
    Ray ray(vec2 ndc) const override;

private:
    vec3 position_{0.0f, 3.0f, 9.0f};
    vec3 target_{0.0f, 1.0f, 0.0f};
    vec3 up_{0.0f, 1.0f, 0.0f};
    float vertical_fov_degrees_ = 40.0f;
    float aspect_ = 16.0f / 9.0f;
};

} // namespace vulchovy
