#include "camera/perspective_camera.h"

#include <cmath>

namespace vulchovy {

void PerspectiveCamera::set_pose(const vec3& position, const vec3& target, const vec3& up) {
    position_ = position;
    target_ = target;
    up_ = up;
}

void PerspectiveCamera::set_vertical_fov(float degrees) {
    vertical_fov_degrees_ = degrees;
}

void PerspectiveCamera::set_aspect(float aspect) {
    aspect_ = aspect;
}

vec3 PerspectiveCamera::forward() const {
    return normalize(target_ - position_);
}

vec3 PerspectiveCamera::right() const {
    return normalize(cross(forward(), up_));
}

vec3 PerspectiveCamera::true_up() const {
    return normalize(cross(right(), forward()));
}

float PerspectiveCamera::tan_half_fov() const {
    return std::tan(radians(vertical_fov_degrees_) * 0.5f);
}

CameraData PerspectiveCamera::gpu_data() const {
    const float tan_half = tan_half_fov();

    CameraData data;
    data.position = vec4(position_, 0.0f);
    data.forward = vec4(forward(), 0.0f);
    data.right = vec4(right() * (tan_half * aspect_), 0.0f);
    data.up = vec4(true_up() * tan_half, 0.0f);
    return data;
}

Ray PerspectiveCamera::ray(vec2 ndc) const {
    Ray result;
    result.origin = position_;
    result.direction = normalize(forward() + right() * (ndc.x * tan_half_fov() * aspect_) +
                                 true_up() * (ndc.y * tan_half_fov()));
    return result;
}

} // namespace vulchovy
