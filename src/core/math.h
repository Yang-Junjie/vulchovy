#pragma once

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace vulchovy {

using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

using glm::cross;
using glm::dot;
using glm::length;
using glm::normalize;

inline float radians(float degrees) {
    return degrees * (glm::pi<float>() / 180.0f);
}

inline float saturate(float value) {
    return glm::clamp(value, 0.0f, 1.0f);
}

} // namespace vulchovy
